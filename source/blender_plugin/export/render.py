import bpy

def set_renderer_params(context, scene):
    scene.render.filepath = "//tmp/render_temp"

    print('lel')

    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.engine = "CYCLES"
    scene.render.use_compositing = True
    scene.render.use_motion_blur = False

    scene.render.film_transparent = False
    scene.view_layers[0].cycles.use_denoising = True

    scene.cycles.use_adaptive_sampling = True
    scene.cycles.samples = 1

    scene.render.use_persistent_data = True

    # Enable GPU acceleration
    scene.cycles.device = "GPU"
    context.preferences.addons["cycles"].preferences.get_devices()
    
    # Let Blender use all available devices, include GPU and CPU
    for d in context.preferences.addons["cycles"].preferences.devices:
        if d.type in ['METAL', 'OPTIX', 'OPENCL']:
            context.preferences.addons["cycles"].preferences.compute_device_type = d.type
            d["use"] = 1
            break
