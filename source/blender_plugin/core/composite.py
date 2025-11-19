import bpy


class CompositeManager:
    _tree = None
    _layer_node = None
    _output_node = None

    def _create_octahedral_encoding_nodes(self, direction_input):
        """
        Creates compositor nodes to encode a 3D direction vector into 2D octahedral coordinates.

        Algorithm:
            n = normalize(n)
            v = n.xy / (abs(n.x) + abs(n.y) + abs(n.z))
            if n.z < 0:
                v = (1 - abs(v.y)) * sign(v.x),
                    (1 - abs(v.x)) * sign(v.y)

        Returns the output socket containing the encoded 2D direction (stored in RG channels).
        """
        links = self._tree.links

        # Step 1: Normalize the input direction
        normalize_node = self._tree.nodes.new(type="CompositorNodeNormalize")
        links.new(direction_input, normalize_node.inputs[0])

        # Step 2: Separate XYZ channels
        separate_xyz = self._tree.nodes.new(type="CompositorNodeSeparateColor")
        separate_xyz.mode = 'RGB'
        links.new(normalize_node.outputs[0], separate_xyz.inputs[0])

        # Step 3: Calculate denominator = abs(n.x) + abs(n.y) + abs(n.z)
        abs_x = self._tree.nodes.new(type="CompositorNodeMath")
        abs_x.operation = 'ABSOLUTE'
        links.new(separate_xyz.outputs[0], abs_x.inputs[0])

        abs_y = self._tree.nodes.new(type="CompositorNodeMath")
        abs_y.operation = 'ABSOLUTE'
        links.new(separate_xyz.outputs[1], abs_y.inputs[0])

        abs_z = self._tree.nodes.new(type="CompositorNodeMath")
        abs_z.operation = 'ABSOLUTE'
        links.new(separate_xyz.outputs[2], abs_z.inputs[0])

        sum_xy = self._tree.nodes.new(type="CompositorNodeMath")
        sum_xy.operation = 'ADD'
        links.new(abs_x.outputs[0], sum_xy.inputs[0])
        links.new(abs_y.outputs[0], sum_xy.inputs[1])

        denominator = self._tree.nodes.new(type="CompositorNodeMath")
        denominator.operation = 'ADD'
        links.new(sum_xy.outputs[0], denominator.inputs[0])
        links.new(abs_z.outputs[0], denominator.inputs[1])

        # Step 4: Calculate v = n.xy / denominator
        v_x = self._tree.nodes.new(type="CompositorNodeMath")
        v_x.operation = 'DIVIDE'
        links.new(separate_xyz.outputs[0], v_x.inputs[0])
        links.new(denominator.outputs[0], v_x.inputs[1])

        v_y = self._tree.nodes.new(type="CompositorNodeMath")
        v_y.operation = 'DIVIDE'
        links.new(separate_xyz.outputs[1], v_y.inputs[0])
        links.new(denominator.outputs[0], v_y.inputs[1])

        # Step 5: Check if n.z < 0
        z_less_than_zero = self._tree.nodes.new(type="CompositorNodeMath")
        z_less_than_zero.operation = 'LESS_THAN'
        links.new(separate_xyz.outputs[2], z_less_than_zero.inputs[0])
        z_less_than_zero.inputs[1].default_value = 0.0

        # Step 6: Calculate wrapped coordinates for n.z < 0 case
        # v.x_wrapped = (1 - abs(v.y)) * sign(v.x)
        abs_v_y = self._tree.nodes.new(type="CompositorNodeMath")
        abs_v_y.operation = 'ABSOLUTE'
        links.new(v_y.outputs[0], abs_v_y.inputs[0])

        one_minus_abs_v_y = self._tree.nodes.new(type="CompositorNodeMath")
        one_minus_abs_v_y.operation = 'SUBTRACT'
        one_minus_abs_v_y.inputs[0].default_value = 1.0
        links.new(abs_v_y.outputs[0], one_minus_abs_v_y.inputs[1])

        sign_v_x = self._tree.nodes.new(type="CompositorNodeMath")
        sign_v_x.operation = 'SIGN'
        links.new(v_x.outputs[0], sign_v_x.inputs[0])

        v_x_wrapped = self._tree.nodes.new(type="CompositorNodeMath")
        v_x_wrapped.operation = 'MULTIPLY'
        links.new(one_minus_abs_v_y.outputs[0], v_x_wrapped.inputs[0])
        links.new(sign_v_x.outputs[0], v_x_wrapped.inputs[1])

        # v.y_wrapped = (1 - abs(v.x)) * sign(v.y)
        abs_v_x = self._tree.nodes.new(type="CompositorNodeMath")
        abs_v_x.operation = 'ABSOLUTE'
        links.new(v_x.outputs[0], abs_v_x.inputs[0])

        one_minus_abs_v_x = self._tree.nodes.new(type="CompositorNodeMath")
        one_minus_abs_v_x.operation = 'SUBTRACT'
        one_minus_abs_v_x.inputs[0].default_value = 1.0
        links.new(abs_v_x.outputs[0], one_minus_abs_v_x.inputs[1])

        sign_v_y = self._tree.nodes.new(type="CompositorNodeMath")
        sign_v_y.operation = 'SIGN'
        links.new(v_y.outputs[0], sign_v_y.inputs[0])

        v_y_wrapped = self._tree.nodes.new(type="CompositorNodeMath")
        v_y_wrapped.operation = 'MULTIPLY'
        links.new(one_minus_abs_v_x.outputs[0], v_y_wrapped.inputs[0])
        links.new(sign_v_y.outputs[0], v_y_wrapped.inputs[1])

        # Step 7: Mix between unwrapped and wrapped based on z < 0 condition
        # Use the comparison result as a factor (0 or 1)
        final_v_x = self._tree.nodes.new(type="CompositorNodeMath")
        final_v_x.operation = 'MULTIPLY'
        final_v_x.use_clamp = False
        links.new(v_x_wrapped.outputs[0], final_v_x.inputs[0])
        links.new(z_less_than_zero.outputs[0], final_v_x.inputs[1])

        one_minus_condition = self._tree.nodes.new(type="CompositorNodeMath")
        one_minus_condition.operation = 'SUBTRACT'
        one_minus_condition.inputs[0].default_value = 1.0
        links.new(z_less_than_zero.outputs[0], one_minus_condition.inputs[1])

        unwrapped_v_x = self._tree.nodes.new(type="CompositorNodeMath")
        unwrapped_v_x.operation = 'MULTIPLY'
        links.new(v_x.outputs[0], unwrapped_v_x.inputs[0])
        links.new(one_minus_condition.outputs[0], unwrapped_v_x.inputs[1])

        result_v_x = self._tree.nodes.new(type="CompositorNodeMath")
        result_v_x.operation = 'ADD'
        links.new(final_v_x.outputs[0], result_v_x.inputs[0])
        links.new(unwrapped_v_x.outputs[0], result_v_x.inputs[1])

        # Same for Y
        final_v_y = self._tree.nodes.new(type="CompositorNodeMath")
        final_v_y.operation = 'MULTIPLY'
        final_v_y.use_clamp = False
        links.new(v_y_wrapped.outputs[0], final_v_y.inputs[0])
        links.new(z_less_than_zero.outputs[0], final_v_y.inputs[1])

        unwrapped_v_y = self._tree.nodes.new(type="CompositorNodeMath")
        unwrapped_v_y.operation = 'MULTIPLY'
        links.new(v_y.outputs[0], unwrapped_v_y.inputs[0])
        links.new(one_minus_condition.outputs[0], unwrapped_v_y.inputs[1])

        result_v_y = self._tree.nodes.new(type="CompositorNodeMath")
        result_v_y.operation = 'ADD'
        links.new(final_v_y.outputs[0], result_v_y.inputs[0])
        links.new(unwrapped_v_y.outputs[0], result_v_y.inputs[1])

        # Step 8: Remap from [-1, 1] to [0, 1] for storage in image format
        # formula: output = (input + 1) * 0.5

        # For X channel
        remap_x_add = self._tree.nodes.new(type="CompositorNodeMath")
        remap_x_add.operation = 'ADD'
        links.new(result_v_x.outputs[0], remap_x_add.inputs[0])
        remap_x_add.inputs[1].default_value = 1.0

        remap_x_mul = self._tree.nodes.new(type="CompositorNodeMath")
        remap_x_mul.operation = 'MULTIPLY'
        links.new(remap_x_add.outputs[0], remap_x_mul.inputs[0])
        remap_x_mul.inputs[1].default_value = 0.5

        # For Y channel
        remap_y_add = self._tree.nodes.new(type="CompositorNodeMath")
        remap_y_add.operation = 'ADD'
        links.new(result_v_y.outputs[0], remap_y_add.inputs[0])
        remap_y_add.inputs[1].default_value = 1.0

        remap_y_mul = self._tree.nodes.new(type="CompositorNodeMath")
        remap_y_mul.operation = 'MULTIPLY'
        links.new(remap_y_add.outputs[0], remap_y_mul.inputs[0])
        remap_y_mul.inputs[1].default_value = 0.5

        # Step 9: Combine the 2D result back into an RGB output (RG channels contain the octahedral coords in [0,1] range)
        combine_rgb = self._tree.nodes.new(type="CompositorNodeCombineColor")
        combine_rgb.mode = 'RGB'
        links.new(remap_x_mul.outputs[0], combine_rgb.inputs[0])  # R
        links.new(remap_y_mul.outputs[0], combine_rgb.inputs[1])  # G
        combine_rgb.inputs[2].default_value = 0.5  # B = 0.5 (neutral value)

        return combine_rgb.outputs[0]

    def __init__(self, scene):
        
        # switch on nodes and get reference
        scene.use_nodes = True

        scene.view_layers["ViewLayer"].use_pass_combined = True
        scene.view_layers["ViewLayer"].use_pass_z = True
        scene.view_layers["ViewLayer"].use_pass_dominant_direction = True

        self._tree = scene.node_tree
        for node in self._tree.nodes:
            self._tree.nodes.remove(node)

        # create input layer node
        self._layer_node = self._tree.nodes.new(type="CompositorNodeRLayers")

        # create output node
        self._output_node = self._tree.nodes.new("CompositorNodeOutputFile")
        self._output_node.format.file_format = "OPEN_EXR"
        self._output_node.format.color_mode = "RGB"
        self._output_node.format.exr_codec = "ZIP"
        self._output_node.format.color_depth = "32"

    def set_main_output(self, output_path):

        self._output_node.file_slots.clear()
        self._output_node.file_slots.new("Color#")

        # add depth slot with BW color mode
        self._output_node.file_slots.new("Depth#")
        depth_slot = self._output_node.file_slots.get("Depth#")
        depth_slot.use_node_format = False
        depth_slot.format.file_format = "OPEN_EXR"
        depth_slot.format.color_mode = "BW"
        depth_slot.format.exr_codec = "ZIP"
        depth_slot.format.color_depth = "32"

        # add preview slot with PNG format
        self._output_node.file_slots.new("Preview#")
        preview_slot = self._output_node.file_slots.get("Preview#")
        preview_slot.use_node_format = False
        preview_slot.format.file_format = "PNG"
        preview_slot.format.color_mode = "RGB"
        preview_slot.format.color_depth = "8"

        # add light direction slot with RGB EXR format
        self._output_node.file_slots.new("LightDirection#")
        light_direction_slot = self._output_node.file_slots.get("LightDirection#")
        light_direction_slot.use_node_format = False
        light_direction_slot.format.file_format = "OPEN_EXR"
        light_direction_slot.format.color_mode = "RGB"
        light_direction_slot.format.exr_codec = "ZIP"
        light_direction_slot.format.color_depth = "32"

        # Create octahedral encoding for the Dominant Direction
        encoded_direction = self._create_octahedral_encoding_nodes(
            self._layer_node.outputs.get("Dominant Direction")
        )

        # link nodes
        links = self._tree.links
        links.clear()
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Color#"))
        links.new(self._layer_node.outputs.get("Depth"), self._output_node.inputs.get("Depth#"))
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Preview#"))
        links.new(encoded_direction, self._output_node.inputs.get("LightDirection#"))

        rel_path = bpy.path.relpath(output_path)
        self._output_node.base_path = rel_path
    
    def set_env_output(self, output_path):

        self._output_node.file_slots.clear()
        self._output_node.file_slots.new("Environment#")

        self._output_node.base_path = bpy.path.relpath(output_path)

        # link nodes
        links = self._tree.links
        links.clear()
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Environment#"))

        rel_path = bpy.path.relpath(output_path)
        self._output_node.base_path = rel_path
