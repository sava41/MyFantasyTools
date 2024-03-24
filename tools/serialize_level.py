import sys
import os
import flatbuffers
import mathutils

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

sys.path.append("./build/generated/")
import mft.data.Vec3
import mft.data.Mat4
import mft.data.View
import mft.data.Level
import mft.data.Triangle


def process_navmesh(scene, views, output_path):
    navmeshes = mfblender.create_navmesh_list(scene)

    if len(navmeshes) > 0:
        navmesh = navmeshes[0]
        # mfblender.export_obj(navmesh.object, output_path)

        for i, view in enumerate(views):
            view.adjacent_views = navmesh.find_adjacent_views(i)
            convex_hull = navmesh.find_convex_hull_area(i)
            view.set_env_probe_location(convex_hull)

        builder = flatbuffers.Builder(4096)

        serialized_views = list()
        for view in views:

            name = builder.CreateString(view.main_camera.name)

            mft.data.View.StartAdjacentViewsVector(builder, len(view.adjacent_views))
            for adj_view in view.adjacent_views:
                builder.PrependUint16(adj_view)
            adjacent_views = builder.EndVector()
            mft.data.View.Start(builder)
            mft.data.View.AddName(builder, name)
            mft.data.View.AddAspect(builder, float(view.res_x) / float(view.res_y))
            mft.data.View.AddFov(builder, view.fov)
            mft.data.View.AddResX(builder, view.res_x)
            mft.data.View.AddResY(builder, view.res_y)
            matrix_world = view.main_camera.matrix_world
            mft.data.View.AddWorldTransform(
                builder,
                mft.data.Mat4.CreateMat4(
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
            mft.data.View.AddAdjacentViews(builder, adjacent_views)
            serialized_view = mft.data.View.End(builder)
            serialized_views.append(serialized_view)

        mft.data.Level.StartViewsVector(builder, len(views))
        for serialized_view in reversed(serialized_views):
            builder.PrependUOffsetTRelative(serialized_view)
        serialized_views = builder.EndVector()

        mft.data.Level.StartNavmeshVertsVector(
            builder, len(navmesh.object.data.vertices)
        )
        for vert in reversed(navmesh.object.data.vertices):
            mft.data.Vec3.CreateVec3(builder, vert.co.x, vert.co.y, vert.co.z)
        navmesh_verts = builder.EndVector()

        mft.data.Level.StartNavmeshTrisVector(
            builder, len(navmesh.object.data.loop_triangles)
        )

        for tri in reversed(navmesh.object.data.loop_triangles):
            view_index = (
                navmesh.object.data.attributes["Views"].data[tri.polygon_index].value
            )
            mft.data.Triangle.CreateTriangle(
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

        mft.data.Level.Start(builder)
        mft.data.Level.AddNavmeshVerts(builder, navmesh_verts)
        mft.data.Level.AddNavmeshTris(builder, navmesh_tris)
        mft.data.Level.AddViews(builder, serialized_views)
        mft.data.Level.AddDataPath(builder, level_data_path)
        mft.data.Level.AddName(builder, level_name)
        level = mft.data.Level.End(builder)

        builder.Finish(level)

        buf = builder.Output()

        if not os.path.exists(output_path):
            os.makedirs(output_path)

        with open(output_path + "\\" + navmesh.name + ".level", "wb") as file:
            file.write(buf)
