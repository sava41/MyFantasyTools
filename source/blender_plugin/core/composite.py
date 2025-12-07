import bpy
import math


class CompositeManager:
    _tree = None
    _layer_node = None
    _output_node = None

    def __init__(self, scene):

        # Blender 5.0+: Create compositor node tree as a separate data block
        scene.view_layers["ViewLayer"].use_pass_combined = True
        scene.view_layers["ViewLayer"].use_pass_z = True
        scene.view_layers["ViewLayer"].use_pass_dominant_direction = True
        scene.view_layers["ViewLayer"].use_pass_normal = True
        scene.view_layers["ViewLayer"].use_pass_diffuse_direct = True
        scene.view_layers["ViewLayer"].use_pass_diffuse_indirect = True
        scene.view_layers["ViewLayer"].use_pass_diffuse_color = True
        scene.view_layers["ViewLayer"].use_pass_glossy_direct = True
        scene.view_layers["ViewLayer"].use_pass_glossy_indirect = True
        scene.view_layers["ViewLayer"].use_pass_glossy_color = True
        scene.view_layers["ViewLayer"].use_pass_transmission_direct = True
        scene.view_layers["ViewLayer"].use_pass_transmission_indirect = True
        scene.view_layers["ViewLayer"].use_pass_transmission_color = True


        # Create new compositor node tree or reuse existing one
        if scene.compositing_node_group is None:
            self._tree = bpy.data.node_groups.new("MFT Compositor", "CompositorNodeTree")
            scene.compositing_node_group = self._tree
        else:
            self._tree = scene.compositing_node_group

        # Clear existing nodes
        for node in self._tree.nodes:
            self._tree.nodes.remove(node)

        # create input layer node
        self._layer_node = self._tree.nodes.new(type="CompositorNodeRLayers")

        # create output node
        self._output_node = self._tree.nodes.new(type="CompositorNodeOutputFile")
        self._output_node.file_name = ""
        self._output_node.format.media_type = "IMAGE"
        self._output_node.format.file_format = "OPEN_EXR"
        self._output_node.format.color_mode = "RGB"
        self._output_node.format.exr_codec = "ZIP"
        self._output_node.format.color_depth = "32"

    def set_main_output(self, output_path):

        self._output_node.file_output_items.clear()
        self._output_node.file_output_items.new("RGBA", "ColorDirect#")
        self._output_node.file_output_items.new("RGBA", "ColorIndirect#")

        # add depth slot with BW color mode
        self._output_node.file_output_items.new("FLOAT", "Depth#")
        depth_slot = self._output_node.file_output_items.get("Depth#")
        depth_slot.override_node_format = True
        depth_slot.format.media_type = "IMAGE"
        depth_slot.format.file_format = "OPEN_EXR"
        depth_slot.format.color_mode = "BW"
        depth_slot.format.exr_codec = "ZIP"
        depth_slot.format.color_depth = "32"

        # add preview slot with PNG format
        self._output_node.file_output_items.new("RGBA", "Preview#")
        preview_slot = self._output_node.file_output_items.get("Preview#")
        preview_slot.override_node_format = True
        preview_slot.format.media_type = "IMAGE"
        preview_slot.format.file_format = "PNG"
        preview_slot.format.color_mode = "RGB"
        preview_slot.format.color_depth = "8"

        # add light direction slot with RGB EXR format
        self._output_node.file_output_items.new("RGBA", "LightDirection#")

        # Octohedral encoding for light direction (RG channels)
        # Plus cone half angle in B channel

        # Create group node for octohedral encoding
        octohedral_group = self._tree.nodes.new(type="CompositorNodeGroup")
        octohedral_group.node_tree = self._create_octohedral_encoding_group()
        
        # Denoise node for light encoding should have hdr off
        denoise_light_direction_node = self._tree.nodes.new(type="CompositorNodeDenoise")
        denoise_light_direction_node.inputs[3].default_value = False

        # Create denoise nodes for ColorDirect and ColorIndirect
        denoise_direct_node = self._tree.nodes.new(type="CompositorNodeDenoise")
        denoise_indirect_node = self._tree.nodes.new(type="CompositorNodeDenoise")

        # Create nodes for ColorDirect calculation
        # ColorDirect = DiffuseDirect * DiffuseColor + GlossyDirect * GlossyColor + TransmissionDirect * TransmissionColor

        # Diffuse direct component
        diffuse_direct_mul = self._tree.nodes.new(type="ShaderNodeMix")
        diffuse_direct_mul.data_type = 'RGBA'
        diffuse_direct_mul.blend_type = "MULTIPLY"
        diffuse_direct_mul.inputs[0].default_value = 1.0  # Factor

        # Glossy direct component
        glossy_direct_mul = self._tree.nodes.new(type="ShaderNodeMix")
        glossy_direct_mul.data_type = 'RGBA'
        glossy_direct_mul.blend_type = "MULTIPLY"
        glossy_direct_mul.inputs[0].default_value = 1.0  # Factor

        # Transmission direct component
        transmission_direct_mul = self._tree.nodes.new(type="ShaderNodeMix")
        transmission_direct_mul.data_type = 'RGBA'
        transmission_direct_mul.blend_type = "MULTIPLY"
        transmission_direct_mul.inputs[0].default_value = 1.0  # Factor

        # Sum diffuse + glossy for direct
        direct_sum1 = self._tree.nodes.new(type="ShaderNodeMix")
        direct_sum1.data_type = 'RGBA'
        direct_sum1.blend_type = "ADD"
        direct_sum1.inputs[0].default_value = 1.0  # Factor

        # Sum (diffuse + glossy) + transmission for direct
        direct_sum2 = self._tree.nodes.new(type="ShaderNodeMix")
        direct_sum2.data_type = 'RGBA'
        direct_sum2.blend_type = "ADD"
        direct_sum2.inputs[0].default_value = 1.0  # Factor

        # Create nodes for ColorIndirect calculation
        # ColorIndirect = DiffuseIndirect * DiffuseColor + GlossyIndirect * GlossyColor + TransmissionIndirect * TransmissionColor

        # Diffuse indirect component
        diffuse_indirect_mul = self._tree.nodes.new(type="ShaderNodeMix")
        diffuse_indirect_mul.data_type = 'RGBA'
        diffuse_indirect_mul.blend_type = "MULTIPLY"
        diffuse_indirect_mul.inputs[0].default_value = 1.0  # Factor

        # Glossy indirect component
        glossy_indirect_mul = self._tree.nodes.new(type="ShaderNodeMix")
        glossy_indirect_mul.data_type = 'RGBA'
        glossy_indirect_mul.blend_type = "MULTIPLY"
        glossy_indirect_mul.inputs[0].default_value = 1.0  # Factor

        # Transmission indirect component
        transmission_indirect_mul = self._tree.nodes.new(type="ShaderNodeMix")
        transmission_indirect_mul.data_type = 'RGBA'
        transmission_indirect_mul.blend_type = "MULTIPLY"
        transmission_indirect_mul.inputs[0].default_value = 1.0  # Factor

        # Sum diffuse + glossy for indirect
        indirect_sum1 = self._tree.nodes.new(type="ShaderNodeMix")
        indirect_sum1.data_type = 'RGBA'
        indirect_sum1.blend_type = "ADD"
        indirect_sum1.inputs[0].default_value = 1.0  # Factor

        # Sum (diffuse + glossy) + transmission for indirect
        indirect_sum2 = self._tree.nodes.new(type="ShaderNodeMix")
        indirect_sum2.data_type = 'RGBA'
        indirect_sum2.blend_type = "ADD"
        indirect_sum2.inputs[0].default_value = 1.0  # Factor

        # link nodes
        links = self._tree.links
        links.clear()

        # ColorDirect links
        links.new(self._layer_node.outputs.get("Diffuse Direct"), diffuse_direct_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Diffuse Color"), diffuse_direct_mul.inputs[7])

        links.new(self._layer_node.outputs.get("Glossy Direct"), glossy_direct_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Glossy Color"), glossy_direct_mul.inputs[7])

        links.new(self._layer_node.outputs.get("Transmission Direct"), transmission_direct_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Transmission Color"), transmission_direct_mul.inputs[7])

        links.new(diffuse_direct_mul.outputs[2], direct_sum1.inputs[6])
        links.new(glossy_direct_mul.outputs[2], direct_sum1.inputs[7])
        links.new(direct_sum1.outputs[2], direct_sum2.inputs[6])
        links.new(transmission_direct_mul.outputs[2], direct_sum2.inputs[7])

        # Denoise ColorDirect
        links.new(direct_sum2.outputs[2], denoise_direct_node.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_direct_node.inputs[2])
        links.new(denoise_direct_node.outputs[0], self._output_node.inputs.get("ColorDirect#"))

        # ColorIndirect links
        links.new(self._layer_node.outputs.get("Diffuse Indirect"), diffuse_indirect_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Diffuse Color"), diffuse_indirect_mul.inputs[7])

        links.new(self._layer_node.outputs.get("Glossy Indirect"), glossy_indirect_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Glossy Color"), glossy_indirect_mul.inputs[7])

        links.new(self._layer_node.outputs.get("Transmission Indirect"), transmission_indirect_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Transmission Color"), transmission_indirect_mul.inputs[7])

        links.new(diffuse_indirect_mul.outputs[2], indirect_sum1.inputs[6])
        links.new(glossy_indirect_mul.outputs[2], indirect_sum1.inputs[7])
        links.new(indirect_sum1.outputs[2], indirect_sum2.inputs[6])
        links.new(transmission_indirect_mul.outputs[2], indirect_sum2.inputs[7])

        # Denoise ColorIndirect
        links.new(indirect_sum2.outputs[2], denoise_indirect_node.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_indirect_node.inputs[2])
        links.new(denoise_indirect_node.outputs[0], self._output_node.inputs.get("ColorIndirect#"))

        # Other outputs
        links.new(self._layer_node.outputs.get("Depth"), self._output_node.inputs.get("Depth#"))
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Preview#"))

        # Link dominant direction and weight to octohedral encoding group
        links.new(self._layer_node.outputs.get("Dominant Direction"), octohedral_group.inputs[0])
        links.new(self._layer_node.outputs.get("Dominant Direction Weight"), octohedral_group.inputs[1])

        # Denoise the encoded result
        links.new(octohedral_group.outputs[0], denoise_light_direction_node.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_light_direction_node.inputs[2])
        links.new(denoise_light_direction_node.outputs[0], self._output_node.inputs.get("LightDirection#"))

        rel_path = bpy.path.relpath(output_path)
        self._output_node.directory = rel_path

    def _create_octohedral_encoding_group(self):
        """Creates a node group that encodes direction vector using octohedral encoding
        and calculates cone half angle from dominant direction weight.

        Inputs:
            0: Dominant Direction (Vector)
            1: Dominant Direction Weight (Float)

        Outputs:
            0: Encoded (RGBA) - RG: octohedral coords, B: cone half angle, A: unused
        """
        # Create new node group
        group = bpy.data.node_groups.new("Octohedral Encoding", "CompositorNodeTree")

        # Create group input/output nodes
        group_input = group.nodes.new(type="NodeGroupInput")
        group_output = group.nodes.new(type="NodeGroupOutput")

        # Define group interface
        group.interface.new_socket("Dominant Direction", in_out='INPUT', socket_type='NodeSocketVector')
        group.interface.new_socket("Dominant Direction Weight", in_out='INPUT', socket_type='NodeSocketFloat')
        group.interface.new_socket("Encoded", in_out='OUTPUT', socket_type='NodeSocketVector')

        # Normalize the input vector
        normalize = group.nodes.new(type="ShaderNodeVectorMath")
        normalize.operation = "NORMALIZE"

        # Separate the normalized vector into X, Y, Z components
        separate_xyz_1 = group.nodes.new(type="ShaderNodeSeparateXYZ")

        # Calculate absolute values for octohedral encoding
        absolute_x = group.nodes.new(type="ShaderNodeMath")
        absolute_x.operation = "ABSOLUTE"

        absolute_y = group.nodes.new(type="ShaderNodeMath")
        absolute_y.operation = "ABSOLUTE"

        absolute_z = group.nodes.new(type="ShaderNodeMath")
        absolute_z.operation = "ABSOLUTE"

        # Add absolute values: |x| + |y|
        add_xy = group.nodes.new(type="ShaderNodeMath")
        add_xy.operation = "ADD"

        # Add |z| to get sum: |x| + |y| + |z|
        add_xyz = group.nodes.new(type="ShaderNodeMath")
        add_xyz.operation = "ADD"

        # Divide vector by sum
        divide_sum = group.nodes.new(type="ShaderNodeVectorMath")
        divide_sum.operation = "DIVIDE"

        # Check if z < 0 (for octohedral unwrapping)
        less_than_z = group.nodes.new(type="ShaderNodeMath")
        less_than_z.operation = "LESS_THAN"

        # Separate the vector vector into X, Y, Z components
        separate_xyz_2 = group.nodes.new(type="ShaderNodeSeparateXYZ")

        # Calculate absolute values of divided x and y
        absolute_div_x = group.nodes.new(type="ShaderNodeMath")
        absolute_div_x.operation = "ABSOLUTE"

        absolute_div_y = group.nodes.new(type="ShaderNodeMath")
        absolute_div_y.operation = "ABSOLUTE"

        # Calculate 1 - |divided_y|
        subtract_y = group.nodes.new(type="ShaderNodeMath")
        subtract_y.operation = "SUBTRACT"
        subtract_y.inputs[0].default_value = 1.0

        # Calculate 1 - |divided_x|
        subtract_x = group.nodes.new(type="ShaderNodeMath")
        subtract_x.operation = "SUBTRACT"
        subtract_x.inputs[0].default_value = 1.0

        # Get sign of x
        sign_x = group.nodes.new(type="ShaderNodeMath")
        sign_x.operation = "SIGN"

        # Get sign of y
        sign_y = group.nodes.new(type="ShaderNodeMath")
        sign_y.operation = "SIGN"

        # Multiply (1 - |divided_y|) * sign(x)
        multiply_x_correction = group.nodes.new(type="ShaderNodeMath")
        multiply_x_correction.operation = "MULTIPLY"

        # Multiply (1 - |divided_x|) * sign(y)
        multiply_y_correction = group.nodes.new(type="ShaderNodeMath")
        multiply_y_correction.operation = "MULTIPLY"

        combine_xyz_1 = group.nodes.new(type="ShaderNodeCombineXYZ")

        # Mix x: blend between divide_x and correction based on z < 0
        mix = group.nodes.new(type="ShaderNodeMix")
        mix.blend_type = "MIX"
        mix.data_type = 'VECTOR'
        mix.clamp_factor = True

        # Remap vector from [-1, 1] to [0, 1]: (x + 1) * 0.5
        add_1 = group.nodes.new(type="ShaderNodeVectorMath")
        add_1.operation = "ADD"
        add_1.inputs[1].default_value = (1.0, 1.0, 1.0)

        multiply_half = group.nodes.new(type="ShaderNodeVectorMath")
        multiply_half.operation = "MULTIPLY"
        multiply_half.inputs[1].default_value = (0.5, 0.5, 0.5)

        separate_xyz_3 = group.nodes.new(type="ShaderNodeSeparateXYZ")

        # Calculate cone half angle from weight
        # Get length of unnormalized input vector
        length = group.nodes.new(type="ShaderNodeVectorMath")
        length.operation = "LENGTH"

        # Divide length by weight
        divide_length_weight = group.nodes.new(type="ShaderNodeMath")
        divide_length_weight.operation = "DIVIDE"
        divide_length_weight.use_clamp = True

        # Calculate acos
        acos = group.nodes.new(type="ShaderNodeMath")
        acos.operation = "ARCCOSINE"

        # Divide by pi/2 to normalize to [0, 1]
        divide_half_pi = group.nodes.new(type="ShaderNodeMath")
        divide_half_pi.operation = "DIVIDE"
        divide_half_pi.inputs[1].default_value = math.pi / 2.0

        # Combine XYZ into final output
        combine_xyz_2 = group.nodes.new(type="ShaderNodeCombineXYZ")

        # Create links
        links = group.links

        # Normalize input
        links.new(group_input.outputs[0], normalize.inputs[0])
        links.new(normalize.outputs[0], separate_xyz_1.inputs[0])

        # Get absolute values
        links.new(separate_xyz_1.outputs[0], absolute_x.inputs[0])
        links.new(separate_xyz_1.outputs[1], absolute_y.inputs[0])
        links.new(separate_xyz_1.outputs[2], absolute_z.inputs[0])

        # Sum absolute values
        links.new(absolute_x.outputs[0], add_xy.inputs[0])
        links.new(absolute_y.outputs[0], add_xy.inputs[1])
        links.new(add_xy.outputs[0], add_xyz.inputs[0])
        links.new(absolute_z.outputs[0], add_xyz.inputs[1])

        # Divide by sum
        links.new(normalize.outputs[0], divide_sum.inputs[0])
        links.new(add_xyz.outputs[0], divide_sum.inputs[1])

        # Calculate corrections for z < 0

        links.new(divide_sum.outputs[0], separate_xyz_2.inputs[0])

        links.new(separate_xyz_2.outputs[0], absolute_div_x.inputs[0])
        links.new(separate_xyz_2.outputs[1], absolute_div_y.inputs[0])

        links.new(absolute_div_y.outputs[0], subtract_y.inputs[1])
        links.new(absolute_div_x.outputs[0], subtract_x.inputs[1])

        links.new(separate_xyz_2.outputs[0], sign_x.inputs[0])
        links.new(separate_xyz_2.outputs[1], sign_y.inputs[0])

        links.new(subtract_y.outputs[0], multiply_x_correction.inputs[0])
        links.new(sign_x.outputs[0], multiply_x_correction.inputs[1])

        links.new(subtract_x.outputs[0], multiply_y_correction.inputs[0])
        links.new(sign_y.outputs[0], multiply_y_correction.inputs[1])

        links.new(multiply_x_correction.outputs[0], combine_xyz_1.inputs[0])
        links.new(multiply_y_correction.outputs[0], combine_xyz_1.inputs[1])

        # Get condition for z < 0
        links.new(separate_xyz_1.outputs[2], less_than_z.inputs[0])

        # Mix based on condition
        links.new(less_than_z.outputs[0], mix.inputs[0])
        links.new(divide_sum.outputs[0], mix.inputs.get('B'))
        links.new(combine_xyz_1.outputs[0], mix.inputs.get('A'))

        # Remap to [0, 1]
        links.new(mix.outputs[1], add_1.inputs[0])
        links.new(add_1.outputs[0], multiply_half.inputs[0])

        # Calculate cone angle
        links.new(group_input.outputs[0], length.inputs[0])
        links.new(length.outputs[1], divide_length_weight.inputs[0])
        links.new(group_input.outputs[1], divide_length_weight.inputs[1])

        links.new(divide_length_weight.outputs[0], acos.inputs[0])
        links.new(acos.outputs[0], divide_half_pi.inputs[0])

        # Combine into final output
        links.new( multiply_half.outputs[0],  separate_xyz_3.inputs[0])
        links.new( separate_xyz_3.outputs[0], combine_xyz_2.inputs[0])
        links.new( separate_xyz_3.outputs[1], combine_xyz_2.inputs[1])
        links.new(divide_half_pi.outputs[0], combine_xyz_2.inputs[2])

        links.new(combine_xyz_2.outputs[0], group_output.inputs[0])

        return group

    def set_env_output(self, output_path):

        self._output_node.file_output_items.clear()
        self._output_node.file_output_items.new("RGBA", "Environment#")

        self._output_node.directory = bpy.path.relpath(output_path)

        # link nodes
        links = self._tree.links
        links.clear()
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Environment#"))

        rel_path = bpy.path.relpath(output_path)
        self._output_node.directory = rel_path
