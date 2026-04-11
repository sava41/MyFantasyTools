import bpy
from bpy.types import Panel, UIList

from ..core import data_models
from ..core.color import CAMERA_COLOR_ATTR

# Camera UIList
class MFT_UL_CameraList(UIList):
    """Cameras"""
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
        box.label(text="Camera List")

        row = box.row()
        row.template_list("MFT_UL_CameraList", "", scene, "mft_cameras",
                         scene, "mft_camera_index", rows=3)

        col = row.column(align=True)
        col.operator("mft.add_camera", icon='ADD', text="")
        col.operator("mft.remove_camera", icon='REMOVE', text="")

        if scene.mft_camera_index >= 0 and scene.mft_camera_index < len(scene.mft_cameras):
            camera_item = scene.mft_cameras[scene.mft_camera_index]
            if camera_item.camera:
                space = context.space_data
                r3d = space.region_3d if space else None
                in_cam_view = (r3d and r3d.view_perspective == 'CAMERA' and
                               space.camera is not None and
                               (space.camera == camera_item.camera or
                                space.camera.name.startswith("_mft_preview_")))
                if in_cam_view:
                    box.operator("mft.set_viewport_camera",
                                 text="Exit Camera View", icon='LOOP_BACK')
                else:
                    box.operator("mft.set_viewport_camera",
                                 text="Look Through Camera", icon='CAMERA_DATA')
                
                box.label(text=f"{camera_item.camera.name} View Properties")
                box.prop(camera_item, "max_pan")
                box.prop(camera_item, "max_tilt")
            else:
                box.label(text="No camera selected")

        box = layout.box()
        box.label(text="Navmesh")
        box.prop(scene.mft_global_settings, "navmesh_object", text="")

        navmesh_obj = scene.mft_global_settings.navmesh_object
        has_colors = (navmesh_obj and
                      navmesh_obj.type == 'MESH' and
                      navmesh_obj.data.color_attributes.get(CAMERA_COLOR_ATTR) is not None)

        shading = context.space_data.shading
        if shading.color_type == 'VERTEX':
            box.operator("mft.toggle_face_color_display",
                            text="Hide Colors", icon='HIDE_ON')
        else:
            box.operator("mft.toggle_face_color_display",
                            text="View Colors", icon='HIDE_OFF')
            if navmesh_obj and not has_colors:
                box.label(text="Assign cameras to faces first", icon='INFO')

        # Show navmesh editing buttons when in edit mode on the navmesh
        if (navmesh_obj and
                context.mode == 'EDIT_MESH' and
                context.edit_object and
                context.edit_object == navmesh_obj):
            box.operator("mft.assign_camera_to_faces",
                         text="Assign Selected Camera to Selected", icon='BRUSH_DATA')

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

            col.progress(text="Rendering...", factor=progress_ratio)
            col.operator("mft.cancel", icon='CANCEL')
        else:
            col.operator("mft.export", icon='RENDER_STILL')


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
