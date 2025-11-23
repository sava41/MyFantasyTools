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
        self._output_node.file_output_items.new("RGBA", "Color#")

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

        # link nodes
        links = self._tree.links
        links.clear()
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Color#"))
        links.new(self._layer_node.outputs.get("Depth"), self._output_node.inputs.get("Depth#"))
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Preview#"))
        links.new(self._layer_node.outputs.get("Dominant Direction"), normalize_node.inputs[0])
        links.new(normalize_node.outputs[0], add_node.inputs[0])
        links.new(add_node.outputs[0], mul_node.inputs[0])
        links.new(mul_node.outputs[0], self._output_node.inputs.get("LightDirection#"))

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
