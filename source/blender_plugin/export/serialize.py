import flatbuffers
import bmesh

from . data import Vec3
from . data import Mat4
from . data import View
from . data import Level
from  .data import Triangle

from ..core import view
from ..core import navmesh as navmesh_module


def serialize_level(navmesh, views) -> bytearray:

    if navmesh is None or views is None:
        return None

    # Create a mapping from camera colors to view indices
    color_to_index = {}
    for view_obj in views:
        color_key = navmesh_module.color_to_comparable(view_obj._camera_color)
        color_to_index[color_key] = view_obj._camera_index

    # Find adjacent views and set env probe locations
    for view_obj in views:
        view_obj._adjacent_views = navmesh.find_adjacent_views(view_obj._camera_color, color_to_index)
        tris = navmesh.get_view_color_tris(view_obj._camera_color)
        view_obj.set_env_probe_location(tris)

    builder = flatbuffers.Builder(4096)

    serialized_views = list()
    for view in views:

        name = builder.CreateString(view._name)

        View.StartAdjacentViewsVector(builder, len(view._adjacent_views))
        for adj_view in view._adjacent_views:
            builder.PrependUint16(adj_view)
        adjacent_views = builder.EndVector()
        View.Start(builder)
        View.AddName(builder, name)
        View.AddAspect(builder, float(view._uncropped_res_y) / float(view._uncropped_res_x))
        View.AddCroppedFov(builder, view._fov)
        View.AddFov(builder, view._uncropped_fov)
        View.AddResX(builder, int(view._uncropped_res_x))
        View.AddResY(builder, int(view._uncropped_res_y))
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

    # Get face colors from the navmesh using bmesh
    bm = bmesh.new()
    bm.from_mesh(navmesh.object.data)
    color_layer = bm.faces.layers.color.get("CameraColor")

    # Build a mapping from polygon index to view index
    polygon_to_view = {}
    if color_layer:
        for face in bm.faces:
            face_color = navmesh_module.color_to_comparable(face[color_layer])
            if face_color in color_to_index:
                polygon_to_view[face.index] = color_to_index[face_color]
            else:
                # Default to 0 if no color match found
                polygon_to_view[face.index] = 0

    bm.free()

    Level.StartNavmeshTrisVector(
        builder, len(navmesh.object.data.loop_triangles)
    )

    for tri in reversed(navmesh.object.data.loop_triangles):
        # Get view index from polygon color mapping
        view_index = polygon_to_view.get(tri.polygon_index, 0)

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
    Level.AddTargetResX(builder, 1920)
    Level.AddTargetResY(builder, 1080)
    level = Level.End(builder)

    builder.Finish(level)

    return builder.Output()
