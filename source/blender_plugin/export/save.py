import bpy
from pathlib import Path
import os

from .. import mftools

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

def convert_all_exr_to_jxl(prefix, input_dir, output_dir):
    for filename in os.listdir(input_dir):
        filepath = Path(input_dir) / filename
        if filepath.is_file() and ".exr" in filename:
            output_file = prefix + "_" + filename.replace("1.exr", ".jxl")
            output_file_path = Path(output_dir) / output_file

            # TODO: exr_to_jxl may hit jxl assert if compiled in debug
            mftools.exr_to_jxl(
                str(filepath.resolve()),
                str(output_file_path.resolve()),
            )