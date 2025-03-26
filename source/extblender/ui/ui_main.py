import bpy
from bpy.types import Panel, Operator, PropertyGroup, UIList
from bpy.props import (
    StringProperty, 
    PointerProperty, 
    CollectionProperty,
    BoolProperty,
    FloatProperty
)
import os

# Custom camera list item
class MFT_CameraListItem(PropertyGroup):
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

# Camera UIList
class MFT_UL_CameraList(UIList):
    """Camera List"""
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            row.prop(item.camera, "name", text="", emboss=False, icon='CAMERA_DATA')
            row.prop(item, "enabled", text="")
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.prop(item.camera, "name", text="", emboss=False, icon='CAMERA_DATA')

# Camera list operators
class MFT_OT_AddCamera(Operator):
    """Add a camera to the list"""
    bl_idname = "mf.add_camera"
    bl_label = "Add Camera"
    bl_description = "Add the selected camera to the list"
    
    def execute(self, context):
        scene = context.scene
        camera_props = scene.mf_cameras
        
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
    bl_idname = "mf.remove_camera"
    bl_label = "Remove Camera"
    bl_description = "Remove the selected camera from the list"
    
    def execute(self, context):
        scene = context.scene
        camera_props = scene.mf_cameras
        
        if scene.mf_camera_index >= 0 and scene.mf_camera_index < len(camera_props):
            camera_props.remove(scene.mf_camera_index)
            scene.mf_camera_index = min(max(0, scene.mf_camera_index - 1), len(camera_props) - 1)
        
        return {'FINISHED'}

# Rendering operator
class MFT_OT_RenderScenes(Operator):
    """Render scenes from all enabled cameras"""
    bl_idname = "mf.render_scenes"
    bl_label = "Export"
    bl_description = "Render images from all enabled cameras to the export path"
    
    def execute(self, context):
        scene = context.scene
        camera_props = scene.mf_cameras
        export_path = scene.mf_export_path
        
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
        if not scene.mf_target_mesh:
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

# Main panel
class MFT_PT_MainPanel(Panel):
    """My Fantasy Tools Main Panel"""
    bl_label = "My Fantasy Tools"
    bl_idname = "MFT_PT_MainPanel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'My Fantasy Tools'
    
    def draw(self, context):
        layout = self.layout
        scene = context.scene
        
        # Camera list section
        box = layout.box()
        box.label(text="Camera List:")
        
        row = box.row()
        row.template_list("MFT_UL_CameraList", "", scene, "mf_cameras", 
                         scene, "mf_camera_index", rows=3)
        
        col = row.column(align=True)
        col.operator("mf.add_camera", icon='ADD', text="")
        col.operator("mf.remove_camera", icon='REMOVE', text="")

        box = layout.box()
        box.label(text=f"Camera: {camera_item.camera.name}")
        
        if scene.mf_camera_index >= 0 and scene.mf_camera_index < len(scene.mf_cameras):
            camera_item = scene.mf_cameras[scene.mf_camera_index]
            box.prop(camera_item, "max_pan")
            box.prop(camera_item, "max_tilt")
        
        # Target mesh section
        box = layout.box()
        box.label(text="Target Object:")
        box.prop(scene, "mf_target_mesh", text="")
        
        # Export path section
        box = layout.box()
        box.label(text="Export Settings:")
        box.prop(scene, "mf_export_path", text="")
        
        # Render button
        layout.operator("mf.render_scenes", icon='RENDER_STILL')

# Property registration function
def register_properties():
    bpy.utils.register_class(MFT_CameraListItem)
    bpy.types.Scene.mf_cameras = CollectionProperty(type=MFT_CameraListItem)
    bpy.types.Scene.mf_camera_index = bpy.props.IntProperty(default=0)
    
    # Target mesh - any mesh object in the scene
    bpy.types.Scene.mf_target_mesh = PointerProperty(
        name="Target Mesh",
        type=bpy.types.Object,
        description="Target mesh for game creation",
        poll=lambda self, obj: obj.type == 'MESH'
    )
    
    # Export path
    bpy.types.Scene.mf_export_path = StringProperty(
        name="Export Path",
        description="Path to export rendered images",
        default="//export",
        subtype='DIR_PATH'
    )

# Property unregistration function
def unregister_properties():
    del bpy.types.Scene.mf_cameras
    del bpy.types.Scene.mf_camera_index
    del bpy.types.Scene.mf_target_mesh
    del bpy.types.Scene.mf_export_path
    bpy.utils.unregister_class(MFT_CameraListItem)

# Registration
classes = (
    MFT_UL_CameraList,
    MFT_OT_AddCamera,
    MFT_OT_RemoveCamera,
    MFT_OT_RenderScenes,
    MFT_PT_MainPanel,
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
