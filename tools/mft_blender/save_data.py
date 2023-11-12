import bpy


def process_navmesh(output_path, scene):
    for ob in scene.objects:
        if ob.type == "MESH" and ob.name.startswith("navmesh_"):
            ob.hide_render = True

            bpy.ops.object.select_all(action="DESELECT")
            ob.select_set(True)

            output_file = output_path + "\\" + ob.name + ".obj"

            bpy.ops.export_scene.obj(
                filepath=output_file,
                use_selection=True,
                use_animation=False,
                use_mesh_modifiers=True,
                use_edges=True,
                use_smooth_groups=False,
                use_smooth_groups_bitflags=False,
                use_normals=False,
                use_uvs=False,
                use_materials=False,
                use_triangles=False,
                use_nurbs=False,
                use_vertex_groups=False,
                use_blen_objects=True,
                group_by_object=False,
                group_by_material=False,
                keep_vertex_order=False,
            )
