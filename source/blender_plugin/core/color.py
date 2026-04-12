import bpy
import bmesh
import random
import colorsys
from bpy.types import Operator

# Name of the mesh color attribute used to store camera assignments
CAMERA_COLOR_ATTR = "CameraColor"

# RGBA value written to faces with no camera assigned
UNASSIGNED_COLOR = (0.0, 0.0, 0.0, 0.0)


# ---------------------------------------------------------------------------
# Pure color utilities
# ---------------------------------------------------------------------------

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
            existing_hsv = colorsys.rgb_to_hsv(*existing_color)
            new_hsv = (hue, saturation, value)

            # Hue is circular, so handle wrapping
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

    # Fallback: return a random color
    hue = random.random()
    r, g, b = colorsys.hsv_to_rgb(hue, 0.75, 0.85)
    return (r, g, b)


def color_to_comparable(color_rgba):
    """
    Convert RGBA color tuple to a comparable format for equality checks.
    Rounds to 3 decimal places to avoid floating point comparison issues.
    """
    return tuple(round(c, 3) for c in color_rgba[:3])


def colors_match(color1, color2, tolerance=0.01):
    """Check if two colors match within a tolerance."""
    if len(color1) < 3 or len(color2) < 3:
        return False
    return all(abs(color1[i] - color2[i]) < tolerance for i in range(3))


def face_color(face, color_layer):
    """Read the CameraColor of a bmesh face from its first loop.

    CameraColor is stored as a CORNER-domain FLOAT_COLOR attribute (one value
    per loop / face-corner).  All loops on the same face share the same colour,
    so reading the first loop is sufficient.
    """
    for loop in face.loops:
        return loop[color_layer]
    return UNASSIGNED_COLOR


# ---------------------------------------------------------------------------
# Operators
# ---------------------------------------------------------------------------

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
        scene = context.scene
        camera_item = scene.mft_cameras[scene.mft_camera_index]

        obj = context.edit_object
        if not obj or obj.type != 'MESH':
            self.report({'ERROR'}, "No mesh object in edit mode")
            return {'CANCELLED'}

        bm = bmesh.from_edit_mesh(obj.data)

        # Ensure CameraColor is a CORNER-domain FLOAT_COLOR attribute so it is
        # visible in the viewport. FLOAT_COLOR stores linear values, matching the
        # FloatVectorProperty, so the viewport and the UI swatch render identically.
        existing = obj.data.color_attributes.get(CAMERA_COLOR_ATTR)
        if existing is not None and (existing.domain != 'CORNER' or existing.data_type != 'FLOAT_COLOR'):
            obj.data.color_attributes.remove(existing)
            existing = None
        if existing is None:
            obj.data.color_attributes.new(name=CAMERA_COLOR_ATTR, type='FLOAT_COLOR', domain='CORNER')

        color_layer = bm.loops.layers.float_color.get(CAMERA_COLOR_ATTR)
        if not color_layer:
            color_layer = bm.loops.layers.float_color.new(CAMERA_COLOR_ATTR)

        camera_color = camera_item.color

        selected_faces = [f for f in bm.faces if f.select]
        if len(selected_faces) == 0:
            self.report({'WARNING'}, "No faces selected")
            return {'CANCELLED'}

        for f in selected_faces:
            for loop in f.loops:
                loop[color_layer] = (camera_color[0], camera_color[1], camera_color[2], 1.0)

        bmesh.update_edit_mesh(obj.data)

        # Ensure CameraColor is the active color attribute so the viewport
        # reflects the change immediately without needing to re-toggle.
        for i, attr in enumerate(obj.data.color_attributes):
            if attr.name == CAMERA_COLOR_ATTR:
                obj.data.color_attributes.active_color_index = i
                break

        self.report({'INFO'}, f"Assigned {camera_item.camera.name} color to {len(selected_faces)} faces")
        return {'FINISHED'}


class MFT_OT_ToggleFaceColorDisplay(Operator):
    """Toggle face color display in viewport"""
    bl_idname = "mft.toggle_face_color_display"
    bl_label = "Toggle Face Color Display"
    bl_description = "Toggle viewport shading to show camera assignment colors on faces"

    @classmethod
    def poll(cls, context):
        if not (context.edit_object and context.edit_object.type == 'MESH'):
            return False
        # Can always toggle colors off; toggling on requires colors to exist
        if context.space_data and context.space_data.shading.color_type == 'VERTEX':
            return True
        return context.edit_object.data.color_attributes.get(CAMERA_COLOR_ATTR) is not None

    def execute(self, context):
        shading = context.space_data.shading

        if shading.color_type == 'VERTEX':
            shading.color_type = 'MATERIAL'
            self.report({'INFO'}, "Face color display disabled")
        else:
            # Guard: don't enable if no colors have been assigned yet — the
            # attribute won't exist and Blender would render the mesh as black.
            obj = context.edit_object
            if not obj or obj.data.color_attributes.get(CAMERA_COLOR_ATTR) is None:
                self.report({'WARNING'}, "No camera colors assigned yet")
                return {'CANCELLED'}

            shading.type = 'SOLID'
            shading.color_type = 'VERTEX'
            for i, attr in enumerate(obj.data.color_attributes):
                if attr.name == CAMERA_COLOR_ATTR:
                    obj.data.color_attributes.active_color_index = i
                    break
            self.report({'INFO'}, "Face color display enabled")

        return {'FINISHED'}


classes = (
    MFT_OT_AssignCameraToFaces,
    MFT_OT_ToggleFaceColorDisplay,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
