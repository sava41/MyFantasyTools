import sys
import os
import shutil
from pathlib import Path
import bpy
import numpy as np

if sys.version_info[0] != 3 and sys.version_info[1] != 10:
    raise Exception("Must be using Python 3.10")

os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import cv2

build_mode = "Release"
bin_path = os.path.abspath("./build/bin/Release/")
if not os.path.isdir(bin_path):
    bin_path = os.path.abspath("./build/bin/Debug/")
    build_mode = "Debug"
if not os.path.isdir(bin_path):
    print("binary path not found. Please build project before using tools")
    quit()
sys.path.append(bin_path)

working_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.append(working_dir_path)

import mftools
import mfblender
import serialize_level


def set_properties(
    scene: bpy.types.Scene,
    resolution_percentage: int = 100,
    output_file_path: str = "",
    res_x: int = 1920,
    res_y: int = 1080,
):
    # scene.render.resolution_percentage = resolution_percentage
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y


def set_cycles_renderer(
    scene: bpy.types.Scene,
    num_samples: int,
    use_denoising: bool = True,
    use_transparent_bg: bool = False,
    prefer_optix_use: bool = True,
    use_adaptive_sampling: bool = False,
):
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.engine = "CYCLES"
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
        bpy.context.preferences.addons[
            "cycles"
        ].preferences.compute_device_type = "OPTIX"

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


if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: output_path resolution_scale num_sample")
        sys.exit(1)

    # Args
    input_file = Path(sys.argv[1])
    output_path_root = Path(sys.argv[2])
    resolution_percentage = int(sys.argv[3])
    num_samples = int(sys.argv[4])

    bpy.ops.wm.open_mainfile(filepath=str(input_file.resolve()))

    output_path_final = output_path_root / "data"

    scene = bpy.data.scenes["Scene"]
    scene.frame_set(0)
    views = mfblender.create_view_list(scene, str(output_path_root.resolve()))

    # Serialize Level
    serialize_level.process_navmesh(scene, views, str(output_path_final.resolve()))

    # Render Settings
    set_cycles_renderer(scene, num_samples)

    # Render
    for view in views:
        view.set_active(scene)
        mfblender.set_main_composite_nodes(view.render_output_path)
        bpy.ops.render.render(scene=scene.name)

        view.set_env_probe_active(scene)
        mfblender.set_env_composite_nodes(view.render_output_path)
        bpy.ops.render.render(scene=scene.name)

    bpy.ops.wm.quit_blender()

    if not (output_path_final / "views").exists():
        os.makedirs(output_path_final / "views")

    for view in views:
        if not Path(view.render_output_path).exists():
            print("Render does not exist: " + view.render_output_path)
            continue
        for filename in os.listdir(view.render_output_path):
            filepath = Path(view.render_output_path) / filename
            if filepath.is_file() and ".exr" in filename:
                output_file = (
                    view.main_camera.name + "_" + filename.replace("0.exr", ".jxl")
                )
                output_file_path = output_path_final / "views" / output_file

                print(f"Processing file: {output_file}")

                image_flags = cv2.IMREAD_ANYDEPTH
                channels = 3

                if "Color" in filename:
                    image_flags = image_flags | cv2.IMREAD_ANYCOLOR
                if "Depth" in filename:
                    image_flags = image_flags | cv2.IMREAD_GRAYSCALE
                    channels = 1
                if "Environment" in filename:
                    image_flags = image_flags | cv2.IMREAD_ANYCOLOR

                img = cv2.imread(str(filepath.resolve()), image_flags)

                # TODO: save_jxl may hit jxl assert if compiled in debug
                mftools.save_jxl(
                    img.shape[1],
                    img.shape[0],
                    channels,
                    img,
                    str(output_file_path.resolve()),
                )

    # Zip data
    shutil.make_archive(
        str((output_path_root / "mft_level_data").resolve()),
        format="zip",
        root_dir=str(output_path_final.resolve()),
    )
