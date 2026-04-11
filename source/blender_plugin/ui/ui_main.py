import bpy
from bpy.types import Panel, UIList

from ..core import data_models

# Camera UIList
class MFT_UL_CameraList(UIList):
    """Camera List"""
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            # Draw colored circle
            color_box = row.row()
            color_box.scale_x = 0.3
            color_box.prop(item, "color", text="")
            # Camera name and enabled checkbox
            row.prop(item.camera, "name", text="", emboss=False, icon='CAMERA_DATA')
            row.prop(item, "enabled", text="")
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.prop(item, "color", text="")
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
        box.label(text="Render Settings:")
        box.prop(scene.mft_global_settings, "render_width")
        box.prop(scene.mft_global_settings, "render_height")
        box.prop(scene.mft_global_settings, "render_samples")

        box = layout.box()
        box.label(text="Export Path:")
        box.prop(scene.mft_global_settings, "export_directory", text="")

        box = layout.box()
        col = layout.column(align=True)

        if scene.mft_global_settings.is_rendering and scene.mft_global_settings.total_views > 0:
            progress_ratio = (min(scene.mft_global_settings.current_view, scene.mft_global_settings.total_views - 1)) / scene.mft_global_settings.total_views
            progress_text = "Rendering..."
            
            col.progress(text=progress_text, factor = progress_ratio)
            
            col.operator("mft.cancel", icon='CANCEL')
        else:
            col.operator("mft.export", icon='RENDER_STILL')


class MFT_PT_NavmeshEditPanel(Panel):
    """Navmesh Face Assignment Panel - shown in edit mode"""
    bl_label = "Navmesh Camera Assignment"
    bl_idname = "MFT_PT_NavmeshEditPanel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'My Fantasy Tools'

    @classmethod
    def poll(cls, context):
        # Only show when in edit mode on a mesh object
        return (context.mode == 'EDIT_MESH' and
                context.edit_object and
                context.edit_object.type == 'MESH')

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # Toggle face color display
        box = layout.box()
        box.label(text="Viewport Display:")
        shading = context.space_data.shading
        if shading.color_type == 'VERTEX':
            box.operator("mft.toggle_face_color_display",
                        text="Hide Face Colors",
                        icon='HIDE_ON')
        else:
            box.operator("mft.toggle_face_color_display",
                        text="Show Face Colors",
                        icon='HIDE_OFF')

        box = layout.box()
        box.label(text="Assign Camera to Selected Faces:")

        # Show camera list
        row = box.row()
        row.template_list("MFT_UL_CameraList", "", scene, "mft_cameras",
                         scene, "mft_camera_index", rows=3)

        # Assign button
        if scene.mft_camera_index >= 0 and scene.mft_camera_index < len(scene.mft_cameras):
            camera_item = scene.mft_cameras[scene.mft_camera_index]
            if camera_item.camera:
                box.operator("mft.assign_camera_to_faces",
                           text=f"Assign {camera_item.camera.name}",
                           icon='BRUSH_DATA')
        else:
            box.label(text="Select a camera to assign")


def register_properties():    
    pass

def unregister_properties():
    pass

classes = (
    MFT_UL_CameraList,
    MFT_PT_MainPanel,
    MFT_PT_NavmeshEditPanel,
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
