import bpy
from bpy.types import Operator


class MFT_OT_RenderScenes(Operator):
    """Render scenes from all enabled cameras"""
    bl_idname = "mft.render_scenes"
    bl_label = "Render"
    bl_description = "Render images from all enabled cameras to the export path"
    
    def execute(self, context):
        scene = context.scene
        camera_props = scene.mft_cameras
        export_path = scene.mft_export_path
        
        # Validate export path
        
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
    pass


def unregister_properties():
    pass

classes = (
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
