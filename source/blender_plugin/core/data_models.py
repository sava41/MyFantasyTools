import bpy
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
