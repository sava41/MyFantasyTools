import bpy
import sys
import math
import os

working_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.append(working_dir_path)

import blendertools

def set_properties(scene: bpy.types.Scene,
                          resolution_percentage: int = 100,
                          output_file_path: str = "",
                          res_x: int = 1920,
                          res_y: int = 1080):
    #scene.render.resolution_percentage = resolution_percentage
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y

def set_cycles_renderer(scene: bpy.types.Scene,
                        num_samples: int,
                        use_denoising: bool = True,
                        use_transparent_bg: bool = False,
                        prefer_optix_use: bool = True,
                        use_adaptive_sampling: bool = False):

    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.engine = 'CYCLES'
    scene.render.use_compositing = True
    scene.render.use_motion_blur = False

    scene.render.film_transparent = use_transparent_bg
    scene.view_layers[0].cycles.use_denoising = use_denoising

    scene.cycles.use_adaptive_sampling = use_adaptive_sampling
    scene.cycles.samples = num_samples

    # Enable GPU acceleration
    # Source - https://blender.stackexchange.com/a/196702
    if prefer_optix_use:
        bpy.context.scene.cycles.device = "GPU"

        # Change the preference setting
        bpy.context.preferences.addons["cycles"].preferences.compute_device_type = "OPTIX"

    # Call get_devices() to let Blender detects GPU device (if any)
    bpy.context.preferences.addons["cycles"].preferences.get_devices()

    # Let Blender use all available devices, include GPU and CPU
    for d in bpy.context.preferences.addons["cycles"].preferences.devices:
        d["use"] = 1

    # Display the devices to be used for rendering
    print("----")
    print("The following devices are avaliable for path tracing:")
    for device in bpy.context.preferences.addons["cycles"].preferences.devices:
        print(" - {}".format(device["name"]))
    print("----")

def setup_cameras() -> bool:
    scene = bpy.context.scene
    index = 0
    scene.frame_start = 0
    
    for ob in scene.objects:
        if ob.type == 'CAMERA':
            marker = scene.timeline_markers.new("{}-ob.name".format(index), frame=index)
            marker.camera = ob
            
            scene.frame_end = index
            index += 1

    if index == 0:
        print("Error: No cameras found" )
        return False

    print("Found {} cameras. Starting renderer.".format(index) )
    print("----")

    return True

working_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.append(working_dir_path)

# Args
output_path = str(sys.argv[sys.argv.index('--') + 1])
resolution_percentage = int(sys.argv[sys.argv.index('--') + 2])
num_samples = int(sys.argv[sys.argv.index('--') + 3])

# Render Settings
scene = bpy.data.scenes["Scene"]
set_properties(scene, resolution_percentage)
set_cycles_renderer(scene, num_samples)
blendertools.enable_composite_nodes(output_path)

if setup_cameras():
    bpy.ops.render.render(animation=True)