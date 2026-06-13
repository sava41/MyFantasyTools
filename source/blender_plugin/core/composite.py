import bpy


class CompositeManager:
    _tree = None
    _layer_node = None
    _output_node = None

    def __init__(self, scene):

        # Blender 5.0+: Create compositor node tree as a separate data block
        scene.view_layers["ViewLayer"].use_pass_combined = True
        scene.view_layers["ViewLayer"].use_pass_z = True
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
        scene.view_layers["ViewLayer"].use_pass_emit = True
        scene.view_layers["ViewLayer"].use_pass_environment = True


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
        self._output_node.file_output_items.new("RGBA", "DirectDiffuse#")
        self._output_node.file_output_items.new("RGBA", "DirectSpecular#")
        self._output_node.file_output_items.new("RGBA", "IndirectDiffuse#")
        self._output_node.file_output_items.new("RGBA", "IndirectSpecular#")
        self._output_node.file_output_items.new("RGBA", "Normal#")

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

        # Denoise nodes — one per denoised output pass
        denoise_direct_diffuse    = self._tree.nodes.new(type="CompositorNodeDenoise")
        denoise_direct_specular   = self._tree.nodes.new(type="CompositorNodeDenoise")
        denoise_indirect_diffuse  = self._tree.nodes.new(type="CompositorNodeDenoise")
        denoise_indirect_specular = self._tree.nodes.new(type="CompositorNodeDenoise")

        # ---- DirectDiffuse = DiffuseDirect×DiffuseColor + TransmissionDirect×TransmissionColor ----
        diffuse_direct_mul = self._tree.nodes.new(type="ShaderNodeMix")
        diffuse_direct_mul.data_type = 'RGBA'
        diffuse_direct_mul.blend_type = "MULTIPLY"
        diffuse_direct_mul.inputs[0].default_value = 1.0

        transmission_direct_mul = self._tree.nodes.new(type="ShaderNodeMix")
        transmission_direct_mul.data_type = 'RGBA'
        transmission_direct_mul.blend_type = "MULTIPLY"
        transmission_direct_mul.inputs[0].default_value = 1.0

        direct_diffuse_add = self._tree.nodes.new(type="ShaderNodeMix")
        direct_diffuse_add.data_type = 'RGBA'
        direct_diffuse_add.blend_type = "ADD"
        direct_diffuse_add.inputs[0].default_value = 1.0

        # ---- DirectSpecular = GlossyDirect×GlossyColor ----
        glossy_direct_mul = self._tree.nodes.new(type="ShaderNodeMix")
        glossy_direct_mul.data_type = 'RGBA'
        glossy_direct_mul.blend_type = "MULTIPLY"
        glossy_direct_mul.inputs[0].default_value = 1.0

        # ---- IndirectDiffuse = DiffuseIndirect×DiffuseColor + TransmissionIndirect×TransmissionColor + Emission + Environment ----
        diffuse_indirect_mul = self._tree.nodes.new(type="ShaderNodeMix")
        diffuse_indirect_mul.data_type = 'RGBA'
        diffuse_indirect_mul.blend_type = "MULTIPLY"
        diffuse_indirect_mul.inputs[0].default_value = 1.0

        transmission_indirect_mul = self._tree.nodes.new(type="ShaderNodeMix")
        transmission_indirect_mul.data_type = 'RGBA'
        transmission_indirect_mul.blend_type = "MULTIPLY"
        transmission_indirect_mul.inputs[0].default_value = 1.0

        indirect_diffuse_add1 = self._tree.nodes.new(type="ShaderNodeMix")
        indirect_diffuse_add1.data_type = 'RGBA'
        indirect_diffuse_add1.blend_type = "ADD"
        indirect_diffuse_add1.inputs[0].default_value = 1.0

        indirect_diffuse_add2 = self._tree.nodes.new(type="ShaderNodeMix")
        indirect_diffuse_add2.data_type = 'RGBA'
        indirect_diffuse_add2.blend_type = "ADD"
        indirect_diffuse_add2.inputs[0].default_value = 1.0

        indirect_diffuse_add3 = self._tree.nodes.new(type="ShaderNodeMix")
        indirect_diffuse_add3.data_type = 'RGBA'
        indirect_diffuse_add3.blend_type = "ADD"
        indirect_diffuse_add3.inputs[0].default_value = 1.0

        # ---- IndirectSpecular = GlossyIndirect×GlossyColor ----
        glossy_indirect_mul = self._tree.nodes.new(type="ShaderNodeMix")
        glossy_indirect_mul.data_type = 'RGBA'
        glossy_indirect_mul.blend_type = "MULTIPLY"
        glossy_indirect_mul.inputs[0].default_value = 1.0

        # link nodes
        links = self._tree.links
        links.clear()

        # DirectDiffuse links
        links.new(self._layer_node.outputs.get("Diffuse Direct"), diffuse_direct_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Diffuse Color"), diffuse_direct_mul.inputs[7])
        links.new(self._layer_node.outputs.get("Transmission Direct"), transmission_direct_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Transmission Color"), transmission_direct_mul.inputs[7])
        links.new(diffuse_direct_mul.outputs[2], direct_diffuse_add.inputs[6])
        links.new(transmission_direct_mul.outputs[2], direct_diffuse_add.inputs[7])
        links.new(direct_diffuse_add.outputs[2], denoise_direct_diffuse.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_direct_diffuse.inputs[2])
        links.new(denoise_direct_diffuse.outputs[0], self._output_node.inputs.get("DirectDiffuse#"))

        # DirectSpecular links
        links.new(self._layer_node.outputs.get("Glossy Direct"), glossy_direct_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Glossy Color"), glossy_direct_mul.inputs[7])
        links.new(glossy_direct_mul.outputs[2], denoise_direct_specular.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_direct_specular.inputs[2])
        links.new(denoise_direct_specular.outputs[0], self._output_node.inputs.get("DirectSpecular#"))

        # IndirectDiffuse links
        links.new(self._layer_node.outputs.get("Diffuse Indirect"), diffuse_indirect_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Diffuse Color"), diffuse_indirect_mul.inputs[7])
        links.new(self._layer_node.outputs.get("Transmission Indirect"), transmission_indirect_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Transmission Color"), transmission_indirect_mul.inputs[7])
        links.new(diffuse_indirect_mul.outputs[2], indirect_diffuse_add1.inputs[6])
        links.new(transmission_indirect_mul.outputs[2], indirect_diffuse_add1.inputs[7])
        links.new(indirect_diffuse_add1.outputs[2], indirect_diffuse_add2.inputs[6])
        links.new(self._layer_node.outputs.get("Emission"), indirect_diffuse_add2.inputs[7])
        links.new(indirect_diffuse_add2.outputs[2], indirect_diffuse_add3.inputs[6])
        links.new(self._layer_node.outputs.get("Environment"), indirect_diffuse_add3.inputs[7])
        links.new(indirect_diffuse_add3.outputs[2], denoise_indirect_diffuse.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_indirect_diffuse.inputs[2])
        links.new(denoise_indirect_diffuse.outputs[0], self._output_node.inputs.get("IndirectDiffuse#"))

        # IndirectSpecular links
        links.new(self._layer_node.outputs.get("Glossy Indirect"), glossy_indirect_mul.inputs[6])
        links.new(self._layer_node.outputs.get("Glossy Color"), glossy_indirect_mul.inputs[7])
        links.new(glossy_indirect_mul.outputs[2], denoise_indirect_specular.inputs[0])
        links.new(self._layer_node.outputs.get("Normal"), denoise_indirect_specular.inputs[2])
        links.new(denoise_indirect_specular.outputs[0], self._output_node.inputs.get("IndirectSpecular#"))

        # Normal: octahedral encoding of world-space normal (RG=[0,1], B=0).
        # Implemented inline (no group) using only nodes confirmed to work in compositor trees.
        # Normal pass is already unit length — no normalize step needed.
        oct_sep = self._tree.nodes.new(type="ShaderNodeSeparateXYZ")
        links.new(self._layer_node.outputs.get("Normal"), oct_sep.inputs[0])

        # |x|, |y|, |z|
        oct_abs_x = self._tree.nodes.new(type="ShaderNodeMath"); oct_abs_x.operation = "ABSOLUTE"
        oct_abs_y = self._tree.nodes.new(type="ShaderNodeMath"); oct_abs_y.operation = "ABSOLUTE"
        oct_abs_z = self._tree.nodes.new(type="ShaderNodeMath"); oct_abs_z.operation = "ABSOLUTE"
        links.new(oct_sep.outputs[0], oct_abs_x.inputs[0])
        links.new(oct_sep.outputs[1], oct_abs_y.inputs[0])
        links.new(oct_sep.outputs[2], oct_abs_z.inputs[0])

        # L1 = |x| + |y| + |z|
        oct_add_xy  = self._tree.nodes.new(type="ShaderNodeMath"); oct_add_xy.operation  = "ADD"
        oct_add_xyz = self._tree.nodes.new(type="ShaderNodeMath"); oct_add_xyz.operation = "ADD"
        links.new(oct_abs_x.outputs[0], oct_add_xy.inputs[0])
        links.new(oct_abs_y.outputs[0], oct_add_xy.inputs[1])
        links.new(oct_add_xy.outputs[0], oct_add_xyz.inputs[0])
        links.new(oct_abs_z.outputs[0], oct_add_xyz.inputs[1])

        # px = x/L1, py = y/L1
        oct_px = self._tree.nodes.new(type="ShaderNodeMath"); oct_px.operation = "DIVIDE"
        oct_py = self._tree.nodes.new(type="ShaderNodeMath"); oct_py.operation = "DIVIDE"
        links.new(oct_sep.outputs[0],    oct_px.inputs[0])
        links.new(oct_add_xyz.outputs[0], oct_px.inputs[1])
        links.new(oct_sep.outputs[1],    oct_py.inputs[0])
        links.new(oct_add_xyz.outputs[0], oct_py.inputs[1])

        # Remap [-1, 1] → [0, 1]: val * 0.5 + 0.5
        # No lower-hemisphere fold — upper-hemisphere-only encoding avoids bilinear seam artifacts.
        oct_remap_x = self._tree.nodes.new(type="ShaderNodeMath"); oct_remap_x.operation = "MULTIPLY_ADD"
        oct_remap_x.inputs[1].default_value = 0.5
        oct_remap_x.inputs[2].default_value = 0.5
        oct_remap_y = self._tree.nodes.new(type="ShaderNodeMath"); oct_remap_y.operation = "MULTIPLY_ADD"
        oct_remap_y.inputs[1].default_value = 0.5
        oct_remap_y.inputs[2].default_value = 0.5
        links.new(oct_px.outputs[0], oct_remap_x.inputs[0])
        links.new(oct_py.outputs[0], oct_remap_y.inputs[0])

        # Combine into output: R=oct_x, G=oct_y, B=0
        oct_comb = self._tree.nodes.new(type="ShaderNodeCombineXYZ")
        links.new(oct_remap_x.outputs[0], oct_comb.inputs[0])
        links.new(oct_remap_y.outputs[0], oct_comb.inputs[1])
        # Z disconnected → 0.0
        links.new(oct_comb.outputs[0], self._output_node.inputs.get("Normal#"))

        # Other outputs
        links.new(self._layer_node.outputs.get("Depth"), self._output_node.inputs.get("Depth#"))
        links.new(self._layer_node.outputs.get("Image"), self._output_node.inputs.get("Preview#"))

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
