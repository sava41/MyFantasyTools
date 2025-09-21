import bpy


class CompositeManager:
    _tree = None
    _layer_node = None
    _output_node = None
    
    def __init__(self, scene):
        
        # switch on nodes and get reference
        scene.use_nodes = True

        scene.view_layers["ViewLayer"].use_pass_combined = True
        scene.view_layers["ViewLayer"].use_pass_z = True

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

        # link nodes
        links = self._tree.links
        links.clear()
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Color#"))
        links.new(self._layer_node.outputs.get("Depth"), self._output_node.inputs.get("Depth#"))
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Preview#"))

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
