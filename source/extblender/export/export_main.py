import bpy
from bpy.types import Operator

import os
from pathlib import Path

from . import render
from . import save
from . import serialize

from ..core.view import *
from ..core.composite import *
from ..core.navmesh import *

class MFT_OT_Export(Operator):
    """export all mft data"""
    bl_idname = "mft.export"
    bl_label = "Export"
    bl_description = "Export mft level and render all backgrounds"

    _timer = None
    _og_scene = None
    _render_scene = None
    _views = []
    _comp_manager = None
    
    
    class RenderState(Enum):
        Camera = 1
        Probe = 2
    
    _render_stage = RenderState.Camera

    def __init__(self):
        pass

    def __del__(self):
        pass

    def modal(self, context, event):
        if event.type == 'TIMER':

            if context.scene.mft_global_settings.cancel_rendering:
                self.cancel(context)
            
            if not context.scene.mft_global_settings.is_rendering:
                
                wm = context.window_manager
                if self._timer:
                    wm.event_timer_remove(self._timer)

                if self._comp_manager:
                    self._comp_manager = None

                if context.scene is self._render_scene:
                    bpy.ops.scene.delete()
                    self._render_scene = None

                if self._og_scene:
                    self._og_scene = None
                
                return {'FINISHED'}

            if not bpy.app.is_job_running('RENDER'):
                index = context.scene.mft_global_settings.current_view

                if index >= len(self._views):
                    context.scene.mft_global_settings.cancel_rendering = True
                    return {'PASS_THROUGH'}

                view = self._views[index]

                if view is None:
                    context.scene.mft_global_settings.cancel_rendering = True
                    return {'PASS_THROUGH'}
                
                view.set_next_camera_active(self._render_scene, self._comp_manager)
                bpy.ops.render.render('INVOKE_DEFAULT', write_still=True, scene=self._render_scene.name)

                if view._current_render is View.RenderType.Complete:
                    context.scene.mft_global_settings.current_view = index + 1

        return {'PASS_THROUGH'}

    def execute(self, context):
        scene = context.scene
        export_path_root = Path(bpy.path.abspath(scene.mft_global_settings.export_directory));
        export_path_final = export_path_root / "data"

        if not os.path.exists(export_path_root):
            os.makedirs(export_path_root)
        
        cameras = scene.mft_cameras

        if context.scene.mft_global_settings.is_rendering:
            # Cancel current render
            context.scene.mft_global_settings.should_cancel = True
            self.report({'INFO'}, "Cancelling render...")
            return {'FINISHED'}

        if not export_path_root:
            self.report({'ERROR'}, "Please specify an export path")
            return {'CANCELLED'}
        
        if not os.path.exists(export_path_root):
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

        self._views = create_view_list(enabled_cameras, str(export_path_root.resolve()))
        navmesh = Navmesh(scene.mft_global_settings.navmesh_object)

        level_data_buffer = serialize.serialize_level(navmesh, self._views)
        with open(export_path_root / (scene.mft_global_settings.navmesh_object.name + ".level"), "wb") as file:
            file.write(level_data_buffer)

        # Create dummy scene for rendering
        self._og_scene = scene
        bpy.ops.scene.new(type='LINK_COPY')
        self._render_scene = bpy.context.scene  # The new scene is now active
        self._render_scene.name = "RenderTempScene"
        context.window.scene = self._render_scene

        self._comp_manager = CompositeManager(self._render_scene)

        # Override render settings
        self._render_scene.render.filepath = "//tmp/render_temp"  # Change as needed

        self._render_scene.render.image_settings.file_format = "OPEN_EXR"
        self._render_scene.render.engine = "CYCLES"
        self._render_scene.render.use_compositing = True
        self._render_scene.render.use_motion_blur = False

        self._render_scene.render.film_transparent = False
        self._render_scene.view_layers[0].cycles.use_denoising = True

        self._render_scene.cycles.use_adaptive_sampling = True
        self._render_scene.cycles.samples = 512

        self._render_scene.render.use_persistent_data = True

        # Enable GPU acceleration
        # Source - https://blender.stackexchange.com/a/196702
        self._render_scene.cycles.device = "GPU"
        context.preferences.addons["cycles"].preferences.get_devices()
        
        # Let Blender use all available devices, include GPU and CPU
        for d in context.preferences.addons["cycles"].preferences.devices:
            if d.type in ['METAL', 'OPTIX', 'OPENCL']:
                context.preferences.addons["cycles"].preferences.compute_device_type = d.type
                d["use"] = 1
                break

        # Render
        self._render_scene.mft_global_settings.current_view = 0
        self._render_scene.mft_global_settings.total_views = len(self._views)
        self._render_scene.mft_global_settings.is_rendering = True
        self._render_scene.mft_global_settings.should_cancel = False

        self._render_stage = self.RenderState.Camera

        wm = context.window_manager
        self._timer = wm.event_timer_add(0.1, window=context.window)
        wm.modal_handler_add(self)
        
        self.report({'INFO'}, f"Starting render of {len(cameras)} views")
        
        return {'RUNNING_MODAL'}

        # if not (output_path_final / "views").exists():
        #     os.makedirs(output_path_final / "views")

        # for view in views:
        #     if not Path(view.render_output_path).exists():
        #         print("Render does not exist: " + view._render_output_path)
        #         continue
        #     for filename in os.listdir(view._render_output_path):
        #         filepath = Path(view._render_output_path) / filename
        #         if filepath.is_file() and ".exr" in filename:
        #             output_file = (
        #                 view._main_camera.name + "_" + filename.replace("0.exr", ".jxl")
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

        return {'FINISHED'}
    
    def cancel(self, context):        
        context.scene.mft_global_settings.is_rendering = False
        context.scene.mft_global_settings.cancel_rendering = False
    
class RENDER_OT_Cancel(Operator):
    """cancel export all mft data"""
    bl_idname = "mft.cancel"
    bl_label = "Cancel"
    bl_description = "Cancel export mft level"
    
    def execute(self, context):
        context.scene.mft_global_settings.cancel_rendering = True
        self.report({'INFO'}, "Render cancelled")
        return {'FINISHED'}

def register_properties():
    pass

def unregister_properties():
    pass

classes = (
    MFT_OT_Export,
    RENDER_OT_Cancel,
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

