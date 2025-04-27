import bpy
from bpy.types import Operator

import os
from pathlib import Path

from . import render
from . import save
from . import serialize

from ..core import view
from ..core.navmesh import Navmesh

class MFT_OT_Export(Operator):
    """export all mft data"""
    bl_idname = "mft.export"
    bl_label = "Export"
    bl_description = "Export mft level and redner all backgrounds"

    def execute(self, context):
            scene = context.scene
            export_path_root = Path(bpy.path.abspath(scene.mft_global_settings.export_directory));
            export_path_final = export_path_root / "data"

            if not os.path.exists(export_path_root):
                os.makedirs(export_path_root)
            
            cameras = scene.mft_cameras

            if not export_path_root:
                self.report({'ERROR'}, "Please specify an export path")
                return {'CANCELLED'}
            
            if not os.path.exists(export_path_root):
                try:
                    os.makedirs(export_path_root)
                except:
                    self.report({'ERROR'}, f"Could not create directory: {export_path_root}")
                    return {'CANCELLED'}
            
            if not scene.mft_global_settings.navmesh_object:
                self.report({'ERROR'}, "Please specify a target mesh object")
                return {'CANCELLED'}
            
            if len(cameras) == 0:
                self.report({'ERROR'}, "No cameras in the list")
                return {'CANCELLED'}
            
            enabled_cameras = [item for item in cameras if item.enabled and item.camera]
            if len(enabled_cameras) == 0:
                self.report({'ERROR'}, "No enabled cameras in the list")
                return {'CANCELLED'}

            # scene = bpy.data.scenes["Scene"]
            # scene.frame_set(0)
            views = view.create_view_list(enabled_cameras, str(export_path_root.resolve()))

            navmesh = Navmesh(scene.mft_global_settings.navmesh_object)

            level_data_buffer = serialize.serialize_level(navmesh, views)

            # Render Settings
            # set_cycles_renderer(scene, num_samples)

            # Render
            # for view in views:
            #     view.set_active(scene)
            #     mfblender.set_main_composite_nodes(view.render_output_path)
            #     bpy.ops.render.render(scene=scene.name)

            #     view.set_env_probe_active(scene)
            #     mfblender.set_env_composite_nodes(view.render_output_path)
            #     bpy.ops.render.render(scene=scene.name)

            # bpy.ops.wm.quit_blender()

            # if not (output_path_final / "views").exists():
            #     os.makedirs(output_path_final / "views")

            # for view in views:
            #     if not Path(view.render_output_path).exists():
            #         print("Render does not exist: " + view.render_output_path)
            #         continue
            #     for filename in os.listdir(view.render_output_path):
            #         filepath = Path(view.render_output_path) / filename
            #         if filepath.is_file() and ".exr" in filename:
            #             output_file = (
            #                 view.main_camera.name + "_" + filename.replace("0.exr", ".jxl")
            #             )
            #             output_file_path = output_path_final / "views" / output_file

            #             print(f"Processing file: {output_file}")

            #             image_flags = cv2.IMREAD_ANYDEPTH
            #             channels = 3

            #             if "Color" in filename:
            #                 image_flags = image_flags | cv2.IMREAD_ANYCOLOR
            #             if "Depth" in filename:
            #                 image_flags = image_flags | cv2.IMREAD_GRAYSCALE
            #                 channels = 1
            #             if "Environment" in filename:
            #                 image_flags = image_flags | cv2.IMREAD_ANYCOLOR

            #             img = cv2.imread(str(filepath.resolve()), image_flags)

            #             # TODO: save_jxl may hit jxl assert if compiled in debug
            #             mftools.save_jxl(
            #                 img.shape[1],
            #                 img.shape[0],
            #                 channels,
            #                 img,
            #                 str(output_file_path.resolve()),
            #             )

            # # Zip data
            # shutil.make_archive(
            #     str((output_path_root / "mft_level_data").resolve()),
            #     format="zip",
            #     root_dir=str(output_path_final.resolve()),
            # )

            # os.rename(
            #     (output_path_root / "mft_level_data.zip").resolve(),
            #     (output_path_root / (input_filename + ".mflevel")).resolve(),
            # )

            with open(export_path_root / (scene.mft_global_settings.navmesh_object.name + ".level"), "wb") as file:
                file.write(level_data_buffer)

            return {'FINISHED'}

def register_properties():
    pass

def unregister_properties():
    pass

classes = (
    MFT_OT_Export,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    register_properties()

def unregister():
    unregister_properties()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

if __name__ == "__main__":
    register()

