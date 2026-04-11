import bpy
import random
import colorsys
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


def generate_distinct_color(existing_colors, min_distance=0.3):
    """
    Generate an aesthetically pleasing color that is distinct from existing colors.
    Uses HSV color space with fixed saturation and value for aesthetic consistency.

    Args:
        existing_colors: List of existing RGB colors as tuples (r, g, b)
        min_distance: Minimum color distance in HSV space

    Returns:
        RGB color tuple (r, g, b)
    """
    max_attempts = 100

    # Use golden ratio for hue distribution to get aesthetically pleasing colors
    golden_ratio_conjugate = 0.618033988749895

    for attempt in range(max_attempts):
        if attempt == 0 and len(existing_colors) == 0:
            # First color: use a nice blue
            hue = 0.6
        else:
            # Use golden ratio to distribute hues evenly
            hue = (len(existing_colors) * golden_ratio_conjugate) % 1.0
            # Add small random offset to avoid exact patterns
            hue = (hue + random.uniform(-0.05, 0.05)) % 1.0

        # Fixed saturation and value for aesthetic consistency
        saturation = random.uniform(0.6, 0.9)
        value = random.uniform(0.7, 0.95)

        # Convert to RGB
        r, g, b = colorsys.hsv_to_rgb(hue, saturation, value)
        new_color = (r, g, b)

        # Check distance from all existing colors
        if len(existing_colors) == 0:
            return new_color

        is_distinct = True
        for existing_color in existing_colors:
            # Convert existing color to HSV for comparison
            existing_hsv = colorsys.rgb_to_hsv(*existing_color)
            new_hsv = (hue, saturation, value)

            # Calculate distance in HSV space
            # Hue is circular, so we need to handle wrapping
            hue_dist = min(abs(new_hsv[0] - existing_hsv[0]),
                          1.0 - abs(new_hsv[0] - existing_hsv[0]))
            sat_dist = abs(new_hsv[1] - existing_hsv[1])
            val_dist = abs(new_hsv[2] - existing_hsv[2])

            # Weighted distance (hue is most important for distinction)
            distance = (hue_dist * 2.0 + sat_dist + val_dist) / 4.0

            if distance < min_distance:
                is_distinct = False
                break

        if is_distinct:
            return new_color

    # If we couldn't find a distinct color, just return a random one
    # This should rarely happen unless there are many cameras
    hue = random.random()
    r, g, b = colorsys.hsv_to_rgb(hue, 0.75, 0.85)
    return (r, g, b)

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
            camera_props.remove(scene.mft_camera_index)
            scene.mft_camera_index = min(max(0, scene.mft_camera_index - 1), len(camera_props) - 1)

        return {'FINISHED'}


class MFT_OT_AssignCameraToFaces(Operator):
    """Assign selected camera to selected navmesh faces"""
    bl_idname = "mft.assign_camera_to_faces"
    bl_label = "Assign Camera to Faces"
    bl_description = "Assign the selected camera's color to the selected navmesh faces"

    @classmethod
    def poll(cls, context):
        return (context.mode == 'EDIT_MESH' and
                context.scene.mft_camera_index >= 0 and
                context.scene.mft_camera_index < len(context.scene.mft_cameras))

    def execute(self, context):
        import bmesh

        scene = context.scene
        camera_item = scene.mft_cameras[scene.mft_camera_index]

        # Get the active mesh object
        obj = context.edit_object
        if not obj or obj.type != 'MESH':
            self.report({'ERROR'}, "No mesh object in edit mode")
            return {'CANCELLED'}

        # Get the bmesh from edit mode
        bm = bmesh.from_edit_mesh(obj.data)

        # Ensure we have a color attribute layer
        if "CameraColor" not in obj.data.attributes:
            obj.data.attributes.new(name="CameraColor", type='BYTE_COLOR', domain='FACE')

        # Get the color attribute layer
        color_layer = bm.faces.layers.color.get("CameraColor")
        if not color_layer:
            color_layer = bm.faces.layers.color.new("CameraColor")

        # Get camera color
        camera_color = camera_item.color

        # Assign color to selected faces
        selected_faces = [f for f in bm.faces if f.select]
        if len(selected_faces) == 0:
            self.report({'WARNING'}, "No faces selected")
            return {'CANCELLED'}

        for face in selected_faces:
            # Blender's color layer uses RGBA values (0-1 range)
            face[color_layer] = (camera_color[0], camera_color[1], camera_color[2], 1.0)

        # Update the mesh
        bmesh.update_edit_mesh(obj.data)

        self.report({'INFO'}, f"Assigned {camera_item.camera.name} color to {len(selected_faces)} faces")
        return {'FINISHED'}


class MFT_OT_ToggleFaceColorDisplay(Operator):
    """Toggle face color display in viewport"""
    bl_idname = "mft.toggle_face_color_display"
    bl_label = "Toggle Face Color Display"
    bl_description = "Toggle viewport shading to show camera assignment colors on faces"

    @classmethod
    def poll(cls, context):
        return context.edit_object and context.edit_object.type == 'MESH'

    def execute(self, context):
        # Get the active shading settings
        shading = context.space_data.shading

        # Toggle color display
        if shading.color_type == 'VERTEX':
            # Turn off - return to default
            shading.color_type = 'MATERIAL'
            self.report({'INFO'}, "Face color display disabled")
        else:
            # Turn on - show vertex colors
            shading.type = 'SOLID'
            shading.color_type = 'VERTEX'
            self.report({'INFO'}, "Face color display enabled")

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
    MFT_OT_AssignCameraToFaces,
    MFT_OT_ToggleFaceColorDisplay,
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