import bpy
from bpy.types import Panel, UIList

from ..core import data_models

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
        row.template_list("MFT_UL_CameraList", "", scene, "mft_cameras", 
                         scene, "mft_camera_index", rows=3)
        
        col = row.column(align=True)
        col.operator("mft.add_camera", icon='ADD', text="")
        col.operator("mft.remove_camera", icon='REMOVE', text="")
        
        if scene.mft_camera_index >= 0 and scene.mft_camera_index < len(scene.mft_cameras):
            camera_item = scene.mft_cameras[scene.mft_camera_index]
            if camera_item.camera:  # Check if camera exists
                box.label(text=f"{camera_item.camera.name} Properties:")
                box.prop(camera_item, "max_pan")
                box.prop(camera_item, "max_tilt")
            else:
                box.label(text="No camera selected")
        
        box = layout.box()
        box.label(text="Nav Mesh:")
        box.prop(scene.mft_global_settings, "navmesh_object", text="")
        
        box = layout.box()
        box.label(text="Export Path:")
        box.prop(scene.mft_global_settings, "export_directory", text="")
        
        layout.operator("mft.render_scenes", icon='RENDER_STILL')

def register_properties():    
    pass

def unregister_properties():
    pass

classes = (
    MFT_UL_CameraList,
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
