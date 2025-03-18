import bpy
from bpy.types import Panel, Operator
import sys
import os

bl_info = {
    "name": "Add Cube Panel",
    "author": "Your Name",
    "version": (1, 0),
    "blender": (3, 0, 0),
    "location": "View3D > UI > Add Cube",
    "description": "Adds a panel with a button to create a cube",
    "category": "3D View",
}

__addon_dir__ = os.path.dirname(os.path.abspath(__file__))

if __addon_dir__ not in sys.path:
    sys.path.append(__addon_dir__)
import mftools

class ADD_OT_cube(Operator):
    """Add a cube to the scene"""
    bl_idname = "object.add_cube_operator"
    bl_label = "Add Cube"
    
    def execute(self, context):
        # Add a cube at the 3D cursor location
        bpy.ops.mesh.primitive_cube_add(location=bpy.context.scene.cursor.location)
        return {'FINISHED'}

class VIEW3D_PT_add_cube_panel(Panel):
    """Creates a Panel in the 3D Viewport UI"""
    bl_label = "Add Cube"
    bl_idname = "VIEW3D_PT_add_cube_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Add Cube'  # This will be the tab name
    
    def draw(self, context):
        layout = self.layout
        layout.operator("object.add_cube_operator")

# Registration
classes = (
    ADD_OT_cube,
    VIEW3D_PT_add_cube_panel,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

if __name__ == "__main__":
    register()