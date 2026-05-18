import flatbuffers
import bmesh
import uuid as uuid_module

from . data import Vec3
from . data import Mat4
from . data import View
from . data import Level
from  .data import Triangle
from  .data import ImageEntry

from ..core import render_view
from ..core import navmesh as navmesh_module
from ..core.color import CAMERA_COLOR_ATTR, color_to_comparable


def _make_image_entry(builder, offset, size, res_x, res_y, channels):
    """Build a FlatBuffer ImageEntry table and return its offset."""
    ImageEntry.Start(builder)
    ImageEntry.AddOffset(builder, offset)
    ImageEntry.AddSize(builder, size)
    ImageEntry.AddResX(builder, res_x)
    ImageEntry.AddResY(builder, res_y)
    ImageEntry.AddChannels(builder, channels)
    return ImageEntry.End(builder)


def serialize_level(navmesh, views, image_entries=None) -> bytearray:
    """Serialize level data to a FlatBuffer bytearray.

    image_entries: optional dict  {view_name: {type_name: (offset, size, res_x, res_y, channels)}}
        where type_name is one of 'ColorDirect', 'ColorIndirect', 'Depth',
        'Environment', 'LightDirection' and offset/size are byte positions
        within the .mflevel image blob.  When None the legacy data_path field
        is written instead (used by the standalone .level format).
    """
    if navmesh is None or views is None:
        return None

    # Create a mapping from camera colors to view indices
    color_to_index = {}
    for view_obj in views:
        color_key = color_to_comparable(view_obj._camera_color)
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

        # Build ImageEntry tables before View.Start (FlatBuffers requires
        # nested table offsets to be created before the parent table starts).
        img_entries = {}
        if image_entries and view._name in image_entries:
            for type_name, (offset, size, res_x, res_y, channels) in image_entries[view._name].items():
                img_entries[type_name] = _make_image_entry(builder, offset, size, res_x, res_y, channels)

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
        if 'ColorDirect'    in img_entries: View.AddColorDirect(builder,    img_entries['ColorDirect'])
        if 'ColorIndirect'  in img_entries: View.AddColorIndirect(builder,  img_entries['ColorIndirect'])
        if 'Depth'          in img_entries: View.AddDepth(builder,          img_entries['Depth'])
        if 'Environment'    in img_entries: View.AddEnvironment(builder,    img_entries['Environment'])
        if 'LightDirection' in img_entries: View.AddLightDirection(builder, img_entries['LightDirection'])
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

    # Get face colors from the navmesh using bmesh.
    # CameraColor is a CORNER-domain color attribute; read via loops layer.
    bm = bmesh.new()
    bm.from_mesh(navmesh.object.data)
    color_layer = bm.loops.layers.float_color.get(CAMERA_COLOR_ATTR)

    # Build a mapping from polygon index to view index
    polygon_to_view = {}
    if color_layer:
        for face in bm.faces:
            # All loops on a face share the same colour; read the first one.
            first_loop_color = next(iter(face.loops))[color_layer]
            face_color = color_to_comparable(first_loop_color)
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
    # data_path is only needed for the legacy .level format
    level_data_path = builder.CreateString("./views/") if image_entries is None else None
    level_uuid = builder.CreateString(str(uuid_module.uuid4()))

    Level.Start(builder)
    Level.AddNavmeshVerts(builder, navmesh_verts)
    Level.AddNavmeshTris(builder, navmesh_tris)
    Level.AddViews(builder, serialized_views)
    if level_data_path is not None:
        Level.AddDataPath(builder, level_data_path)
    Level.AddName(builder, level_name)
    Level.AddTargetResX(builder, 1920)
    Level.AddTargetResY(builder, 1080)
    Level.AddUuid(builder, level_uuid)
    level = Level.End(builder)

    builder.Finish(level, file_identifier=b'MFLV')

    return builder.Output()
