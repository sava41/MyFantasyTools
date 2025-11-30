import bpy


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

        # Remap the world-space direction vector from [-1, 1] to [0, 1] for storage
        # Formula: output = (input + 1) * 0.5

        normalize_node = self._tree.nodes.new(type="ShaderNodeVectorMath")
        normalize_node.operation = "NORMALIZE"

        # Add 1.0 to all channels
        add_node = self._tree.nodes.new(type="ShaderNodeVectorMath")
        add_node.operation = "ADD"
        add_node.inputs[1].default_value = (1.0, 1.0, 1.0)  # Add 1.0 to each channel

        # Multiply by 0.5
        mul_node = self._tree.nodes.new(type="ShaderNodeVectorMath")
        mul_node.operation = "MULTIPLY"
        mul_node.inputs[1].default_value = (0.5, 0.5, 0.5)  # Multiply by 0.5

        denoise_node = self._tree.nodes.new(type="CompositorNodeDenoise")

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
        links.new(self._layer_node.outputs.get("Dominant Direction"), normalize_node.inputs[0])
        links.new(normalize_node.outputs[0], add_node.inputs[0])
        links.new(add_node.outputs[0], mul_node.inputs[0])
        links.new(mul_node.outputs[0], denoise_node.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_node.inputs[2])
        links.new(denoise_node.outputs[0], self._output_node.inputs.get("LightDirection#"))

        rel_path = bpy.path.relpath(output_path)
        self._output_node.directory = rel_path
    
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
