import sys
import math
import os
from pathlib import Path
import bpy
import numpy as np

os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import cv2

build_mode = "Release"
bin_path = os.path.abspath('./build/bin/Release/')
if not os.path.isdir(bin_path):
    bin_path = os.path.abspath('./build/bin/Debug/')
    build_mode = "Debug"
if not os.path.isdir(bin_path):
    print("binary path not found. Please build project before using tools")
    quit()
sys.path.append(bin_path)

working_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.append(working_dir_path)

import mft_tools
import mft_blender

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

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: output_path resolution_scale num_sample")
        sys.exit(1)

    # Args
    input_file = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    resolution_percentage = int(sys.argv[3])
    num_samples = int(sys.argv[4])

    bpy.ops.wm.open_mainfile(filepath=str(input_file.resolve()))

    # Render Settings
    scene = bpy.data.scenes["Scene"]
    set_properties(scene, resolution_percentage)
    set_cycles_renderer(scene, num_samples)
    mft_blender.enable_composite_nodes(str(output_path.resolve()))

    #if setup_cameras():
        #bpy.ops.render.render(animation=True)

    bpy.ops.wm.quit_blender()

    # Convert render data to jxl
    for filename in os.listdir(output_path.resolve()):
        filepath = output_path / filename
        if filepath.is_file() and ".exr" in filename:
            print(f"Processing file: {filename}")
            output_file = output_path / filename.replace("exr", "jxl")

            image_flags = cv2.IMREAD_ANYDEPTH
            channels = 3
            
            if("Color" in filename):
                image_flags = image_flags | cv2.IMREAD_ANYCOLOR
            if("Depth" in filename):
                image_flags = image_flags | cv2.IMREAD_GRAYSCALE 
                channels = 1

            img = cv2.imread(str(filepath.resolve()), image_flags)

            print(img.shape, channels)
            mft_tools.save_jxl(img.shape[1], img.shape[0], channels, img, str(output_file.resolve()))