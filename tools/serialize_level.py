import sys
import os
import flatbuffers

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

import mft_tools
import mft_blender

sys.path.append("./build/generated_flatbuffers/")
import MFT.Vec3
import MFT.Level


def process_navmesh(scene, output_path):
    for ob in scene.objects:
        if ob.type == "MESH" and ob.name.startswith("navmesh_"):
            mft_blender.export_obj(ob, output_path)
            mesh = ob.data
            name = ob.name.removeprefix("navmesh_")

            builder = flatbuffers.Builder(1024)

            level_name = builder.CreateString(name)

            MFT.Level.StartPathVector(builder, len(mesh.vertices))
            for vert in mesh.vertices:
                MFT.Vec3.CreateVec3(builder, vert.co.x, vert.co.y, vert.co.z)
            navmesh_verts = builder.EndVector()

            MFT.Level.Start(builder)
            MFT.Level.AddPath(builder, navmesh_verts)
            MFT.Level.AddName(builder, level_name)
            level = MFT.Level.End(builder)

            builder.Finish(level)

            buf = builder.Output()

            with open(output_path + "\\" + name + ".level", "wb") as file:
                file.write(buf)

            break
