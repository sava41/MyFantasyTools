import bpy
import math
from bpy.types import Operator, PropertyGroup
from bpy.props import (
    StringProperty,
    PointerProperty,
    FloatProperty,
    BoolProperty,
    CollectionProperty,
    IntProperty,
    FloatVectorProperty
)

import bmesh

from . color import generate_distinct_color, CAMERA_COLOR_ATTR, UNASSIGNED_COLOR, colors_match, face_color
from . view import compute_uncropped_fov


# ---------------------------------------------------------------------------
# Frustum preview state
# ---------------------------------------------------------------------------

_preview_state = None


def _setup_render_border(scene, uncropped_fov, uncropped_vfov, fov, vfov):
    """Set render border to show the original FOV rect inside the expanded view."""
    frac_x = math.tan(fov * 0.5) / math.tan(uncropped_fov * 0.5)
    frac_y = math.tan(vfov * 0.5) / math.tan(uncropped_vfov * 0.5)
    scene.render.use_border = True
    scene.render.use_crop_to_border = False
    scene.render.border_min_x = (1.0 - frac_x) * 0.5
    scene.render.border_max_x = (1.0 + frac_x) * 0.5
    scene.render.border_min_y = (1.0 - frac_y) * 0.5
    scene.render.border_max_y = (1.0 + frac_y) * 0.5


def _teardown_preview():
    """Remove the preview camera and restore all saved state."""
    global _preview_state

    if _preview_state is None:
        return

    state = _preview_state
    _preview_state = None  # clear first to prevent re-entry from handlers

    preview_obj = state['preview_obj']
    original_camera = state['original_camera']

    # Restore space.camera in every viewport that is still showing the preview
    try:
        for window in bpy.context.window_manager.windows:
            for area in window.screen.areas:
                if area.type != 'VIEW_3D':
                    continue
                for sp in area.spaces:
                    if sp.type == 'VIEW_3D' and sp.camera is preview_obj:
                        sp.camera = original_camera
    except Exception:
        pass

    # Remove preview objects
    try:
        bpy.data.objects.remove(preview_obj, do_unlink=True)
    except Exception:
        pass
    try:
        bpy.data.cameras.remove(state['preview_cam_data'])
    except Exception:
        pass

    # Restore render border
    try:
        scene = bpy.data.scenes.get(state['scene_name'])
        if scene:
            scene.render.use_border = state['saved_use_border']
            scene.render.use_crop_to_border = state['saved_use_crop']
            scene.render.border_min_x = state['saved_border_min_x']
            scene.render.border_max_x = state['saved_border_max_x']
            scene.render.border_min_y = state['saved_border_min_y']
            scene.render.border_max_y = state['saved_border_max_y']
    except Exception:
        pass

    # Unregister handlers
    _unregister_preview_handlers()


def _unregister_preview_handlers():
    if _preview_depsgraph_handler in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.remove(_preview_depsgraph_handler)
    if bpy.app.timers.is_registered(_preview_exit_timer):
        bpy.app.timers.unregister(_preview_exit_timer)


def _preview_depsgraph_handler(scene, depsgraph):
    """Sync preview camera transform and update FOV when params change."""
    if _preview_state is None:
        return

    state = _preview_state
    preview_obj = state['preview_obj']
    original_camera = state['original_camera']
    camera_item = state['camera_item']

    # If the user selected a different camera in the MFT list, switch the preview
    cameras = scene.mft_cameras
    idx = scene.mft_camera_index
    if 0 <= idx < len(cameras):
        selected_item = cameras[idx]
        if selected_item.camera and selected_item.camera is not original_camera:
            original_camera = selected_item.camera
            camera_item = selected_item
            state['original_camera'] = original_camera
            state['camera_item'] = camera_item
            preview_obj.data.clip_start = original_camera.data.clip_start
            preview_obj.data.clip_end = original_camera.data.clip_end
            # Force parameter update below by making tracked values mismatch
            state['tracked_fov'] = -1.0
            state['tracked_pan'] = -1.0
            state['tracked_tilt'] = -1.0

    # Sync world transform so preview camera follows the original camera
    if preview_obj.matrix_world != original_camera.matrix_world:
        preview_obj.matrix_world = original_camera.matrix_world.copy()

    # Check if any tracked parameter has changed
    current_fov = original_camera.data.angle
    current_pan = camera_item.max_pan
    current_tilt = camera_item.max_tilt
    res_x = scene.mft_global_settings.render_width
    res_y = scene.mft_global_settings.render_height

    if (abs(current_fov - state['tracked_fov']) > 1e-6 or
            abs(current_pan - state['tracked_pan']) > 1e-6 or
            abs(current_tilt - state['tracked_tilt']) > 1e-6 or
            res_x != state['tracked_res_x'] or
            res_y != state['tracked_res_y']):
        fov = original_camera.data.angle
        aspect = float(res_y) / float(res_x)
        uncropped_fov, uncropped_vfov, vfov = compute_uncropped_fov(
            fov, aspect, current_pan, current_tilt
        )
        preview_obj.data.angle = uncropped_fov
        scene.render.resolution_x = max(1, int(
            res_x * math.tan(uncropped_fov * 0.5) / math.tan(fov * 0.5)))
        scene.render.resolution_y = max(1, int(
            res_y * math.tan(uncropped_vfov * 0.5) / math.tan(vfov * 0.5)))
        if current_pan > 0.0 or current_tilt > 0.0:
            _setup_render_border(scene, uncropped_fov, uncropped_vfov, fov, vfov)
        else:
            # No expansion — restore the original border state
            scene.render.use_border = state['saved_use_border']
            scene.render.use_crop_to_border = state['saved_use_crop']
            scene.render.border_min_x = state['saved_border_min_x']
            scene.render.border_max_x = state['saved_border_max_x']
            scene.render.border_min_y = state['saved_border_min_y']
            scene.render.border_max_y = state['saved_border_max_y']
        state['tracked_fov'] = current_fov
        state['tracked_pan'] = current_pan
        state['tracked_tilt'] = current_tilt
        state['tracked_res_x'] = res_x
        state['tracked_res_y'] = res_y


def _preview_exit_timer():
    """Poll for viewport exiting camera view via non-button means."""
    if _preview_state is None:
        return None  # stop timer

    preview_obj = _preview_state['preview_obj']

    try:
        for window in bpy.context.window_manager.windows:
            for area in window.screen.areas:
                if area.type != 'VIEW_3D':
                    continue
                for sp in area.spaces:
                    if sp.type == 'VIEW_3D':
                        if (sp.region_3d.view_perspective == 'CAMERA' and
                                sp.camera is preview_obj):
                            return 0.2  # still active — check again later
    except Exception:
        pass

    # Not active in any viewport → clean up
    _teardown_preview()
    return None  # stop timer


class MFT_Camera(PropertyGroup):
    """Group of properties representing a camera in the list"""
    camera: PointerProperty(
        name="Camera",
        type=bpy.types.Object,
        description="A camera object in the scene",
        poll=lambda self, obj: obj.type == 'CAMERA'
    )
    enabled: BoolProperty(
        name="Enabled",
        description="Use this camera for rendering",
        default=True
    )
    max_pan: FloatProperty(
        name="Max Pan",
        description="Maximum pan angle in degrees",
        default=0.0,
        min=0.0,
        max=360.0,
        subtype='ANGLE'
    )
    max_tilt: FloatProperty(
        name="Max Tilt",
        description="Maximum tilt angle in degrees",
        default=0.0,
        min=0.0,
        max=90.0,
        subtype='ANGLE'
    )
    color: FloatVectorProperty(
        name="Camera Color",
        description="Color associated with this camera for navmesh face assignment",
        subtype='COLOR',
        size=3,
        min=0.0,
        max=1.0,
        default=(1.0, 1.0, 1.0)
    )

class MFT_GlobalSettings(PropertyGroup):
    """Group of global properties for the addon"""
    export_directory: StringProperty(
        name="Export Directory",
        description="Directory to export files to",
        subtype='DIR_PATH',
        default="//" # Default to blend file directory
    )
    navmesh_object: PointerProperty(
        name="Navmesh Object",
        description="The mesh object to use as the navigation mesh",
        type=bpy.types.Object,
        poll=lambda self, obj: obj.type == 'MESH'
    )

    render_width: IntProperty(
        name="Resolution Width",
        description="Render target resolution width in pixels",
        default=1920,
        min=1,
        soft_max=7680
    )
    render_height: IntProperty(
        name="Resolution Height",
        description="Render target resolution height in pixels",
        default=1080,
        min=1,
        soft_max=4320
    )
    render_samples: IntProperty(
        name="Samples",
        description="Number of render samples to use",
        default=128,
        min=1,
        max=1024
    )

    current_view: IntProperty(default=0)
    total_views: IntProperty(default=0)
    is_rendering: BoolProperty(default=False)
    cancel_rendering: BoolProperty(default=False)

class MFT_OT_AddCamera(Operator):
    """Add a camera to the list"""
    bl_idname = "mft.add_camera"
    bl_label = "Add Camera"
    bl_description = "Add the selected camera to the list"

    def execute(self, context):
        scene = context.scene
        camera_props = scene.mft_cameras

        # Check if active object is a camera
        active_obj = context.active_object
        if not active_obj or active_obj.type != 'CAMERA':
            self.report({'ERROR'}, "Please select a camera object")
            return {'CANCELLED'}

        # Check if camera is already in list
        for item in camera_props:
            if item.camera == active_obj:
                self.report({'INFO'}, "Camera already in list")
                return {'CANCELLED'}

        # Get existing colors to ensure uniqueness
        existing_colors = [tuple(item.color) for item in camera_props]

        # Add to list
        item = camera_props.add()
        item.camera = active_obj

        # Assign a distinct, aesthetically pleasing color
        item.color = generate_distinct_color(existing_colors)

        return {'FINISHED'}

class MFT_OT_RemoveCamera(Operator):
    """Remove a camera from the list"""
    bl_idname = "mft.remove_camera"
    bl_label = "Remove Camera"
    bl_description = "Remove the selected camera from the list"

    def execute(self, context):
        scene = context.scene
        camera_props = scene.mft_cameras

        if scene.mft_camera_index >= 0 and scene.mft_camera_index < len(camera_props):
            removed_color = tuple(camera_props[scene.mft_camera_index].color)

            # Clear this camera's color from navmesh faces before removing
            navmesh_obj = scene.mft_global_settings.navmesh_object
            if navmesh_obj and navmesh_obj.type == 'MESH':
                mesh = navmesh_obj.data
                in_edit = (context.mode == 'EDIT_MESH' and context.edit_object == navmesh_obj)
                bm = bmesh.from_edit_mesh(mesh) if in_edit else bmesh.new()
                if not in_edit:
                    bm.from_mesh(mesh)

                color_layer = bm.loops.layers.float_color.get(CAMERA_COLOR_ATTR)
                has_remaining = False
                if color_layer:
                    for f in bm.faces:
                        fc = face_color(f, color_layer)
                        if colors_match(fc, removed_color):
                            for loop in f.loops:
                                loop[color_layer] = UNASSIGNED_COLOR
                        elif not colors_match(fc, UNASSIGNED_COLOR):
                            has_remaining = True

                if in_edit:
                    bmesh.update_edit_mesh(mesh)
                else:
                    bm.to_mesh(mesh)
                    mesh.update()
                    bm.free()

                # If no face assignments remain, turn off vertex color display
                if not has_remaining:
                    for space in context.area.spaces:
                        if space.type == 'VIEW_3D' and space.shading.color_type == 'VERTEX':
                            space.shading.color_type = 'MATERIAL'
                            break

            camera_props.remove(scene.mft_camera_index)
            scene.mft_camera_index = min(max(0, scene.mft_camera_index - 1), len(camera_props) - 1)

        return {'FINISHED'}


class MFT_OT_SetViewportCamera(Operator):
    """Toggle looking through the selected camera in the viewport"""
    bl_idname = "mft.set_viewport_camera"
    bl_label = "Toggle Viewport Camera"
    bl_description = "Look through the selected camera in the viewport, or return to perspective"

    @classmethod
    def poll(cls, context):
        return (context.space_data and
                context.space_data.type == 'VIEW_3D' and
                context.scene.mft_camera_index >= 0 and
                context.scene.mft_camera_index < len(context.scene.mft_cameras) and
                bool(context.scene.mft_cameras[context.scene.mft_camera_index].camera))

    def execute(self, context):
        global _preview_state

        camera_item = context.scene.mft_cameras[context.scene.mft_camera_index]
        camera_obj = camera_item.camera
        space = context.space_data
        r3d = space.region_3d
        scene = context.scene

        preview_name = f"_mft_preview_{camera_obj.name}"
        in_cam_view = (
            r3d.view_perspective == 'CAMERA' and
            space.camera is not None and
            (space.camera is camera_obj or
             space.camera.name == preview_name or
             (_preview_state is not None and space.camera is _preview_state['preview_obj']))
        )

        if in_cam_view:
            _teardown_preview()
            r3d.view_perspective = 'PERSP'
        else:
            # Clean up any leftover preview before starting a new one
            _teardown_preview()

            res_x = scene.mft_global_settings.render_width
            res_y = scene.mft_global_settings.render_height
            fov = camera_obj.data.angle
            aspect = float(res_y) / float(res_x)
            uncropped_fov, uncropped_vfov, vfov = compute_uncropped_fov(
                fov, aspect, camera_item.max_pan, camera_item.max_tilt
            )

            # Always create an orphan preview camera so the depsgraph handler
            # can respond to pan/tilt/fov changes even when they start at zero
            preview_cam_data = bpy.data.cameras.new(preview_name)
            preview_cam_data.angle = uncropped_fov
            preview_cam_data.type = 'PERSP'
            preview_cam_data.clip_start = camera_obj.data.clip_start
            preview_cam_data.clip_end = camera_obj.data.clip_end

            preview_obj = bpy.data.objects.new(preview_name, preview_cam_data)
            preview_obj.matrix_world = camera_obj.matrix_world.copy()

            _preview_state = {
                'preview_obj': preview_obj,
                'preview_cam_data': preview_cam_data,
                'original_camera': camera_obj,
                'camera_item': camera_item,
                'scene_name': scene.name,
                'tracked_fov': camera_obj.data.angle,
                'tracked_pan': camera_item.max_pan,
                'tracked_tilt': camera_item.max_tilt,
                'tracked_res_x': res_x,
                'tracked_res_y': res_y,
                'saved_use_border': scene.render.use_border,
                'saved_use_crop': scene.render.use_crop_to_border,
                'saved_border_min_x': scene.render.border_min_x,
                'saved_border_max_x': scene.render.border_max_x,
                'saved_border_min_y': scene.render.border_min_y,
                'saved_border_max_y': scene.render.border_max_y,
            }

            scene.render.resolution_x = max(1, int(
                res_x * math.tan(uncropped_fov * 0.5) / math.tan(fov * 0.5)))
            scene.render.resolution_y = max(1, int(
                res_y * math.tan(uncropped_vfov * 0.5) / math.tan(vfov * 0.5)))

            if camera_item.max_pan > 0.0 or camera_item.max_tilt > 0.0:
                _setup_render_border(scene, uncropped_fov, uncropped_vfov, fov, vfov)

            if _preview_depsgraph_handler not in bpy.app.handlers.depsgraph_update_post:
                bpy.app.handlers.depsgraph_update_post.append(_preview_depsgraph_handler)
            if not bpy.app.timers.is_registered(_preview_exit_timer):
                bpy.app.timers.register(_preview_exit_timer, first_interval=0.2)

            space.camera = preview_obj
            r3d.view_perspective = 'CAMERA'

        return {'FINISHED'}


def register_properties():
    bpy.types.Scene.mft_global_settings = PointerProperty(type=MFT_GlobalSettings)
    bpy.types.Scene.mft_cameras = CollectionProperty(type=MFT_Camera)
    bpy.types.Scene.mft_camera_index = IntProperty(default=0)


def unregister_properties():
    del bpy.types.Scene.mft_global_settings
    del bpy.types.Scene.mft_cameras
    del bpy.types.Scene.mft_camera_index

classes = (
    MFT_GlobalSettings,
    MFT_Camera,
    MFT_OT_AddCamera,
    MFT_OT_RemoveCamera,
    MFT_OT_SetViewportCamera,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    register_properties()

def unregister():
    _teardown_preview()
    unregister_properties()
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

if __name__ == "__main__":
    register()
