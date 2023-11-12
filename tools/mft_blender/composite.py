import bpy

def enable_composite_nodes(output_path, enabled = True):

    # switch on nodes and get reference
    bpy.context.scene.use_nodes = enabled

    if enabled:

        bpy.context.scene.view_layers["ViewLayer"].use_pass_combined = True
        bpy.context.scene.view_layers["ViewLayer"].use_pass_z = True

        tree = bpy.context.scene.node_tree

        # clear default nodes
        for node in tree.nodes:
            tree.nodes.remove(node)

        # create input layer node
        layer_node = tree.nodes.new(type="CompositorNodeRLayers")
        layer_node.location = 0,0

        # create output node
        output_node = tree.nodes.new("CompositorNodeOutputFile")
        
        output_node.format.file_format = 'OPEN_EXR'
        output_node.format.color_mode = 'RGB'
        output_node.format.exr_codec = 'ZIP'
        output_node.format.color_depth = '32'
        
        output_node.file_slots.clear()
        output_node.file_slots.new("Color")
        output_node.file_slots.new("Depth")

        output_node.base_path = bpy.path.relpath(output_path)

        # create preview node
        preview_node = tree.nodes.new("CompositorNodeOutputFile")
        
        preview_node.format.file_format = 'PNG'
        preview_node.format.color_mode = 'RGB'
        
        preview_node.file_slots.clear()
        preview_node.file_slots.new("Preview")

        preview_node.base_path = bpy.path.relpath(output_path)

        # link nodes
        links = tree.links
        links.new(layer_node.outputs.get("Image"), preview_node.inputs.get("Preview"))
        links.new(layer_node.outputs.get("Image"), output_node.inputs.get("Color"))
        links.new(layer_node.outputs.get("Depth"), output_node.inputs.get("Depth"))