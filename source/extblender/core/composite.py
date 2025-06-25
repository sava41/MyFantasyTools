import bpy


class CompositeManager:
    _tree = None
    _layer_node = None
    _output_node = None
    _preview_node = None
    
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

        # create preview node
        self._preview_node = self._tree.nodes.new("CompositorNodeOutputFile")
        self._preview_node.format.file_format = "PNG"
        self._preview_node.format.color_mode = "RGB"

        self._preview_node.file_slots.clear()
        self._preview_node.file_slots.new("Preview#")

    def set_main_output(self, output_path):

        self._output_node.file_slots.clear()
        self._output_node.file_slots.new("Color#")
        self._output_node.file_slots.new("Depth#")

        # link nodes
        links = self._tree.links
        links.clear()
        links.new(self._layer_node.outputs.get("Image"), self._preview_node.inputs.get("Preview#"))
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Color#"))
        links.new(self._layer_node.outputs.get("Depth"), self._output_node.inputs.get("Depth#"))

        rel_path = bpy.path.relpath(output_path)
        self._output_node.base_path = rel_path
        self._preview_node.base_path = rel_path
    
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
