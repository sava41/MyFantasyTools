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


def process_navmesh(scene, cameras, output_path):
    navmeshes = mfblender.create_navmesh_list(scene)

    if len(navmeshes) > 0:
        navmesh = navmeshes[0]
        # mfblender.export_obj(navmesh.object, output_path)

        for i, camera in enumerate(cameras):
            camera.adjacent_views = navmesh.find_adjacent_views(i)
            convex_hull = navmesh.find_convex_hull_area(i)

            trace_start = camera.main_camera.location
            trace_end = trace_start + (
                camera.main_camera.matrix_world.to_quaternion()
                @ mathutils.Vector((0.0, 0.0, -10000.0))
            )

            point_close = trace_end
            point_far = trace_start

            for i in range(len(convex_hull)):
                point_a = mathutils.Vector((convex_hull[i][0], convex_hull[i][1], 0))
                point_b = mathutils.Vector(
                    (convex_hull[i - 1][0], convex_hull[i - 1][1], 0)
                )
                plane_normal = mathutils.Vector((0.0, 0.0, 1.0)).cross(
                    point_a - point_b
                )

                intersection = mathutils.geometry.intersect_line_plane(
                    trace_start, trace_end, point_a, plane_normal
                )

                if intersection:
                    ab_length = point_a - point_b
                    ab_length.z = 0
                    ab_length = ab_length.length
                    ia_length = intersection - point_a
                    ia_length.z = 0
                    ia_length = ia_length.length
                    ib_length = intersection - point_b
                    ib_length.z = 0
                    ib_length = ib_length.length

                    if ia_length <= ab_length and ib_length <= ab_length:
                        if (point_close - trace_start).length > (
                            intersection - trace_start
                        ).length:
                            point_close = intersection
                        if (point_far - trace_start).length < (
                            intersection - trace_start
                        ).length:
                            point_far = intersection

            camera.set_env_probe_location((point_close + point_far) * 0.5)

        builder = flatbuffers.Builder(4096)

        views = list()
        for camera in cameras:

            camera_name = builder.CreateString(camera.main_camera.name)

            mft.data.View.StartAdjacentViewsVector(builder, len(camera.adjacent_views))
            for adj_view in camera.adjacent_views:
                builder.PrependUint16(adj_view)
            adjacent_views = builder.EndVector()
            mft.data.View.Start(builder)
            mft.data.View.AddName(builder, camera_name)
            mft.data.View.AddAspect(builder, float(camera.res_x) / float(camera.res_y))
            mft.data.View.AddFov(builder, camera.fov)
            mft.data.View.AddResX(builder, camera.res_x)
            mft.data.View.AddResY(builder, camera.res_y)
            matrix_world = camera.main_camera.matrix_world
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
            view = mft.data.View.End(builder)
            views.append(view)

        mft.data.Level.StartViewsVector(builder, len(cameras))
        for view in reversed(views):
            builder.PrependUOffsetTRelative(view)
        views = builder.EndVector()

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
        mft.data.Level.AddViews(builder, views)
        mft.data.Level.AddDataPath(builder, level_data_path)
        mft.data.Level.AddName(builder, level_name)
        level = mft.data.Level.End(builder)

        builder.Finish(level)

        buf = builder.Output()

        if not os.path.exists(output_path):
            os.makedirs(output_path)

        with open(output_path + "\\" + navmesh.name + ".level", "wb") as file:
            file.write(buf)
