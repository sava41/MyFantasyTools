import bpy


def export_obj(mesh_object, output_path) -> bool:
    if mesh_object.type == "MESH":
        bpy.ops.object.select_all(action="DESELECT")
        mesh_object.select_set(True)

        output_file = output_path + "\\" + mesh_object.name + ".obj"

        bpy.ops.wm.obj_export(
            filepath=output_file,
            export_selected_objects=True,
            export_uv=False,
            export_normals=False,
            export_colors=False,
            export_materials=False,
            export_triangulated_mesh=True,
            export_smooth_groups=False,
        )

        return True

    return False
