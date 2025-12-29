import bpy
from bpy.types import Operator

import os
from pathlib import Path
import shutil
import threading

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
    _jxl_threads = []
    _comp_manager = None
    _next_index = False

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

                for area in bpy.context.screen.areas:
                    if area.type == 'IMAGE_EDITOR':
                        area.type = 'VIEW_3D'

                output_path_root = Path(bpy.path.abspath(context.scene.mft_global_settings.export_directory))
                output_path_final = output_path_root / "data"
                
                for t in self._jxl_threads:
                    t.join()
                
                # Zip data
                shutil.make_archive(
                    str((output_path_root / "mft_level_data").resolve()),
                    format="zip",
                    root_dir=str(output_path_final.resolve()),
                )

                level_name = bpy.path.display_name_from_filepath(bpy.data.filepath)

                if not level_name:
                    level_name = "new_level"

                os.rename(
                    (output_path_root / "mft_level_data.zip").resolve(),
                    (output_path_root / (level_name + ".mflevel")).resolve(),
                )
                
                return {'FINISHED'}

            if not bpy.app.is_job_running('RENDER'):
                
                if self._next_index:
                    prev_view = self._views[context.scene.mft_global_settings.current_view]

                    output_path_root = Path(bpy.path.abspath(context.scene.mft_global_settings.export_directory))
                    output_path_final = output_path_root / "data" / "views"

                    os.makedirs(output_path_final, exist_ok=True)

                    t = threading.Thread(target=save.convert_all_exr_to_jxl, args=(prev_view._name, prev_view._render_output_path, str(output_path_final.resolve())))
                    t.start()

                    self._jxl_threads.append(t)
                    
                    context.scene.mft_global_settings.current_view += 1
                    self._next_index = False

                index = context.scene.mft_global_settings.current_view

                if index >= len(self._views):
                    context.scene.mft_global_settings.cancel_rendering = True
                    return {'PASS_THROUGH'}

                view = self._views[index]

                if view is None:
                    context.scene.mft_global_settings.cancel_rendering = True
                    return {'PASS_THROUGH'}
                
                view.set_next_camera_active(self._render_scene, self._comp_manager)
                bpy.ops.render.render('INVOKE_DEFAULT', write_still=False, scene=self._render_scene.name)

                if view._current_render is View.RenderType.Complete:
                    self._next_index = True

        return {'PASS_THROUGH'}

    def execute(self, context):
        scene = context.scene
        export_path_root = Path(bpy.path.abspath(scene.mft_global_settings.export_directory))
        export_path_final = export_path_root / "data"
        
        cameras = scene.mft_cameras

        if scene.mft_global_settings.is_rendering:
            # Cancel current render
            scene.mft_global_settings.should_cancel = True
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

        self._views = create_view_list(enabled_cameras, str(export_path_root.resolve()), scene)
        navmesh = Navmesh(scene.mft_global_settings.navmesh_object)

        os.makedirs(export_path_final, exist_ok=True)

        level_data_buffer = serialize.serialize_level(navmesh, self._views)
        with open(export_path_final / (scene.mft_global_settings.navmesh_object.name + ".level"), "wb") as file:
            file.write(level_data_buffer)

        # Create dummy scene for rendering
        self._og_scene = scene
        bpy.ops.scene.new(type='LINK_COPY')
        self._render_scene = bpy.context.scene  # The new scene is now active
        self._render_scene.name = "RenderTempScene"
        context.window.scene = self._render_scene

        self._comp_manager = CompositeManager(self._render_scene)

        # Override render settings
        render.set_renderer_params(context, self._render_scene)

        # Render
        self._render_scene.mft_global_settings.current_view = 0
        self._render_scene.mft_global_settings.total_views = len(self._views)
        self._render_scene.mft_global_settings.is_rendering = True
        self._render_scene.mft_global_settings.should_cancel = False

        wm = context.window_manager
        self._timer = wm.event_timer_add(0.2, window=context.window)
        wm.modal_handler_add(self)
        
        self.report({'INFO'}, f"Starting render of {len(cameras)} views")
        
        return {'RUNNING_MODAL'}
    
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
