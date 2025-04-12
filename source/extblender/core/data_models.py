import bpy
from bpy.types import Operator, PropertyGroup
from bpy.props import (
    StringProperty,
    PointerProperty,
    FloatProperty,
    BoolProperty,
    CollectionProperty,
    IntProperty
)

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
        default=45.0,
        min=0.0,
        max=360.0,
        subtype='ANGLE'
    )
    max_tilt: FloatProperty(
        name="Max Tilt",
        description="Maximum tilt angle in degrees",
        default=30.0,
        min=0.0,
        max=90.0,
        subtype='ANGLE'
    )

# --- Added Global Settings Class ---
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
        
        # Add to list
        item = camera_props.add()
        item.camera = active_obj
        
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

# Rendering operator
class MFT_OT_RenderScenes(Operator):
    """Render scenes from all enabled cameras"""
    bl_idname = "mft.render_scenes"
    bl_label = "Export"
    bl_description = "Render images from all enabled cameras to the export path"
    
    def execute(self, context):
        scene = context.scene
        camera_props = scene.mft_cameras
        export_path = scene.mft_export_path
        
        # Validate export path
        if not export_path:
            self.report({'ERROR'}, "Please specify an export path")
            return {'CANCELLED'}
        
        if not os.path.exists(export_path):
            try:
                os.makedirs(export_path)
            except:
                self.report({'ERROR'}, f"Could not create directory: {export_path}")
                return {'CANCELLED'}
        
        # Check for target mesh
        if not scene.mft_target_mesh:
            self.report({'ERROR'}, "Please specify a target mesh object")
            return {'CANCELLED'}
        
        # Check for cameras
        if len(camera_props) == 0:
            self.report({'ERROR'}, "No cameras in the list")
            return {'CANCELLED'}
        
        # Count enabled cameras
        enabled_cameras = [item for item in camera_props if item.enabled and item.camera]
        if len(enabled_cameras) == 0:
            self.report({'ERROR'}, "No enabled cameras in the list")
            return {'CANCELLED'}
        
        # Store original settings
        original_camera = scene.camera
        
        # Do the rendering for each enabled camera
        rendered_count = 0
        for index, item in enumerate(camera_props):
            if not item.enabled or not item.camera:
                continue
                
            # Set camera as active
            scene.camera = item.camera
            
            # Set output path
            camera_name = item.camera.name
            output_path = os.path.join(export_path, f"{camera_name}.png")
            scene.render.filepath = output_path
            
            # Render
            bpy.ops.render.render(write_still=True)
            rendered_count += 1
        
        # Restore original camera
        scene.camera = original_camera
        
        self.report({'INFO'}, f"Rendered {rendered_count} images to {export_path}")
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
    MFT_OT_RenderScenes,
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