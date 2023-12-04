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
import MFT.Mat4
import MFT.View
import MFT.Level
import MFT.Triangle


def process_navmesh(scene, cameras, output_path):
    navmeshes = mft_blender.create_navmesh_list(scene)

    if len(navmeshes) > 0:
        # mft_blender.export_obj(ob, output_path)
        navmesh = navmeshes[0]

        for i, camera in enumerate(cameras):
            camera.adjacent_views = navmesh.find_adjacent_views(i)

        builder = flatbuffers.Builder(4096)

        views = list()
        for camera in cameras:
            camera_name = builder.CreateString(camera.object.name)

            MFT.View.StartAdjacentViewsVector(builder, len(camera.adjacent_views))
            for adj_view in camera.adjacent_views:
                builder.PrependUint16(adj_view)
            adjacent_views = builder.EndVector()
            MFT.View.Start(builder)
            MFT.View.AddName(builder, camera_name)
            MFT.View.AddAspect(builder, float(camera.res_x) / float(camera.res_y))
            MFT.View.AddResX(builder, camera.res_x)
            MFT.View.AddResY(builder, camera.res_y)
            matrix_world = camera.object.matrix_world
            MFT.View.AddWorldTransform(
                builder,
                MFT.Mat4.CreateMat4(
                    builder,
                    matrix_world[0][0],
                    matrix_world[0][1],
                    matrix_world[0][2],
                    matrix_world[0][3],
                    matrix_world[1][0],
                    matrix_world[1][1],
                    matrix_world[1][2],
                    matrix_world[1][3],
                    matrix_world[2][0],
                    matrix_world[2][1],
                    matrix_world[2][2],
                    matrix_world[2][3],
                    matrix_world[3][0],
                    matrix_world[3][1],
                    matrix_world[3][2],
                    matrix_world[3][3],
                ),
            )
            MFT.View.AddAdjacentViews(builder, adjacent_views)
            view = MFT.View.End(builder)
            views.append(view)

        MFT.Level.StartViewsVector(builder, len(cameras))
        for view in views:
            builder.PrependUOffsetTRelative(view)
        views = builder.EndVector()

        MFT.Level.StartNavmeshVertsVector(builder, len(navmesh.object.data.vertices))
        for vert in reversed(navmesh.object.data.vertices):
            MFT.Vec3.CreateVec3(builder, vert.co.x, vert.co.y, vert.co.z)
        navmesh_verts = builder.EndVector()

        MFT.Level.StartNavmeshTrisVector(builder, len(navmesh.object.data.vertices))
        for tri in reversed(navmesh.object.data.loop_triangles):
            view_index = (
                navmesh.object.data.attributes["Views"].data[tri.polygon_index].value
            )
            MFT.Triangle.CreateTriangle(
                builder,
                tri.vertices[0],
                tri.vertices[1],
                tri.vertices[2],
                view_index,
                0,
            )
        navmesh_tris = builder.EndVector()

        level_name = builder.CreateString(navmesh.name)
        level_data_path = builder.CreateString("./views/")

        MFT.Level.Start(builder)
        MFT.Level.AddNavmeshVerts(builder, navmesh_verts)
        MFT.Level.AddNavmeshTris(builder, navmesh_tris)
        MFT.Level.AddViews(builder, views)
        MFT.Level.AddDataPath(builder, level_data_path)
        MFT.Level.AddName(builder, level_name)
        level = MFT.Level.End(builder)

        builder.Finish(level)

        buf = builder.Output()

        with open(output_path + "\\" + navmesh.name + ".level", "wb") as file:
            file.write(buf)
