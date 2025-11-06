import flatbuffers

from . data import Vec3
from . data import Mat4
from . data import View
from . data import Level
from  .data import Triangle

from ..core import view
from ..core import navmesh


def serialize_level(navmesh, views) -> bytearray:

    if navmesh is None or views is None:
        return None

    for i, view in enumerate(views):
        view._adjacent_views = navmesh.find_adjacent_views(i)
        convex_hull = navmesh.find_convex_hull_area(i)
        view.set_env_probe_location(convex_hull)

    builder = flatbuffers.Builder(4096)

    serialized_views = list()
    for view in views:

        name = builder.CreateString(view._main_camera.name)

        View.StartAdjacentViewsVector(builder, len(view._adjacent_views))
        for adj_view in view._adjacent_views:
            builder.PrependUint16(adj_view)
        adjacent_views = builder.EndVector()
        View.Start(builder)
        View.AddName(builder, name)
        View.AddAspect(builder, float(view._res_x) / float(view._res_y))
        View.AddFov(builder, view._fov)
        View.AddResX(builder, view._res_x)
        View.AddResY(builder, view._res_y)
        View.AddMaxPan(builder, view._max_pan)
        View.AddMaxTilt(builder, view._max_tilt)
        matrix_world = view._main_camera.matrix_world
        View.AddWorldTransform(
            builder,
            Mat4.CreateMat4(
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
        View.AddAdjacentViews(builder, adjacent_views)
        serialized_view = View.End(builder)
        serialized_views.append(serialized_view)

    Level.StartViewsVector(builder, len(views))
    for serialized_view in reversed(serialized_views):
        builder.PrependUOffsetTRelative(serialized_view)
    serialized_views = builder.EndVector()

    Level.StartNavmeshVertsVector(
        builder, len(navmesh.object.data.vertices)
    )
    for vert in reversed(navmesh.object.data.vertices):
        Vec3.CreateVec3(builder, vert.co.x, vert.co.y, vert.co.z)
    navmesh_verts = builder.EndVector()

    Level.StartNavmeshTrisVector(
        builder, len(navmesh.object.data.loop_triangles)
    )

    for tri in reversed(navmesh.object.data.loop_triangles):
        view_index = (
            navmesh.object.data.attributes["Views"].data[tri.polygon_index].value
        )
        Triangle.CreateTriangle(
            builder,
            tri.vertices[0],
            tri.vertices[1],
            tri.vertices[2],
            view_index,
            0,
        )
    navmesh_tris = builder.EndVector()

    level_name = builder.CreateString(navmesh.object.name)
    level_data_path = builder.CreateString("./views/")

    Level.Start(builder)
    Level.AddNavmeshVerts(builder, navmesh_verts)
    Level.AddNavmeshTris(builder, navmesh_tris)
    Level.AddViews(builder, serialized_views)
    Level.AddDataPath(builder, level_data_path)
    Level.AddName(builder, level_name)
    level = Level.End(builder)

    builder.Finish(level)

    return builder.Output()
