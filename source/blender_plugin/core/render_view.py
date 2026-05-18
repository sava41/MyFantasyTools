import bpy
import mathutils
import math
from enum import Enum

from .composite import *


class RenderType(Enum):
    Main = 1
    Probe = 2
    Complete = 3

    def next(self):
        members = list(self.__class__)
        index = members.index(self)
        return members[index + 1] if index + 1 < len(members) else None


def compute_uncropped_fov(fov, aspect, max_pan, max_tilt):
    """Compute the expanded FOV needed to accommodate max pan/tilt rotation.

    Rotates each corner of the original frustum by max_tilt then max_pan and
    finds the maximum angular extents, giving the smallest enclosing FOV.

    Args:
        fov:      Original horizontal FOV in radians
        aspect:   Render aspect ratio (height / width)
        max_pan:  Maximum pan angle in radians
        max_tilt: Maximum tilt angle in radians

    Returns:
        (uncropped_fov, uncropped_vfov, vfov) all in radians
    """
    vfov = 2.0 * math.atan(math.tan(fov * 0.5) * aspect)
    hfov_half = fov * 0.5
    vfov_half = vfov * 0.5

    max_angle_h = 0.0
    max_angle_v = 0.0

    for corner_h in (-hfov_half, hfov_half):
        for corner_v in (-vfov_half, vfov_half):
            corner_dir = mathutils.Vector((
                math.sin(corner_h) * math.cos(corner_v),
                math.sin(corner_v),
                -math.cos(corner_h) * math.cos(corner_v),
            ))
            rot_pan = mathutils.Matrix.Rotation(max_pan, 3, 'Y')
            rot_tilt = mathutils.Matrix.Rotation(max_tilt, 3, 'X')
            rotated = rot_pan @ rot_tilt @ corner_dir

            horiz = math.sqrt(rotated.x ** 2 + rotated.z ** 2)
            angle_h = abs(math.atan2(rotated.x, -rotated.z))
            angle_v = abs(math.atan2(rotated.y, horiz))

            max_angle_h = max(max_angle_h, angle_h)
            max_angle_v = max(max_angle_v, angle_v)

    return max_angle_h * 2.0, max_angle_v * 2.0, vfov


def place_env_probe(env_camera_obj, main_camera, navmesh_data):
    """Place env_camera_obj at the midpoint between the camera and navmesh bounds.

    Raycasts from main_camera toward the navmesh perimeter and floor tris to
    find the closest and farthest intersection points, then positions the env
    probe camera at their midpoint.
    """
    tris, convex_hull, = navmesh_data

    trace_dir = main_camera.matrix_world.to_quaternion() @ mathutils.Vector((0.0, 0.0, -10000.0))
    trace_start = main_camera.location
    trace_end = trace_start + trace_dir

    point_close = trace_end
    point_far = trace_start

    # Ray trace vertical walls of the prism
    for i in range(len(convex_hull)):
        point_a = mathutils.Vector((convex_hull[i][0], convex_hull[i][1], 0))
        point_b = mathutils.Vector(
            (convex_hull[i - 1][0], convex_hull[i - 1][1], 0)
        )
        plane_normal = mathutils.Vector((0.0, 0.0, 1.0)).cross(point_a - point_b)

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

    # Ray trace navmesh tris
    for tri in tris:
        intersection = mathutils.geometry.intersect_ray_tri(tri.verts[0], tri.verts[1], tri.verts[2], trace_dir, trace_start)

        if intersection:
            point_far = intersection

    if point_close is trace_end:
        point_close = trace_start

    if point_far is trace_start:
        point_far = trace_start

    env_camera_obj.location = (point_close + point_far) * 0.5


class RenderView:
    _main_camera = None
    _name = ""
    _env_camera = None
    _env_camera_obj = None
    _res_x = 1920
    _res_y = 1080
    _uncropped_res_x = 1920
    _uncropped_res_y = 1080
    _aspect = 1.0
    _fov = 90
    _uncropped_fov = 90
    _render_output_path = ""
    _adjacent_views = {}

    _max_pan = 0
    _max_tilt = 0
    _camera_index = 0
    _camera_color = (1.0, 1.0, 1.0)
    _env_res_x = 1024
    _env_res_y = 512

    _current_render = RenderType.Main

    def __init__(self, object, camera_index, output_path, scene):

        self._res_x = scene.mft_global_settings.render_width
        self._res_y = scene.mft_global_settings.render_height

        self._max_pan = object.max_pan
        self._max_tilt = object.max_tilt
        self._camera_index = camera_index
        self._camera_color = tuple(object.color)

        camera = object.camera

        self._name = camera.name
        self._main_camera = camera.copy()
        self._main_camera.data = camera.data.copy()
        self._aspect = float(self._res_y) / float(self._res_x)

        #horizontal fov
        self._fov = camera.data.angle
        self._render_output_path = output_path + "//renders//" + camera.name

        self._uncropped_fov, uncropped_vfov, vfov = compute_uncropped_fov(
            camera.data.angle, self._aspect, object.max_pan, object.max_tilt
        )

        self._uncropped_res_x = self._res_x / self._fov * self._uncropped_fov
        self._uncropped_res_y = self._res_y / vfov * uncropped_vfov

        self._main_camera.data.angle = self._uncropped_fov
        self._main_camera.data.type = "PERSP"

        self._env_camera = bpy.data.cameras.new(camera.name + "_env")
        self._env_camera_obj = bpy.data.objects.new(camera.name + "_env", self._env_camera)

        bpy.context.scene.collection.objects.link(self._env_camera_obj)
        self._env_camera_obj.rotation_euler[0] = math.radians(90)
        self._env_camera_obj.data.type = "PANO"
        self._env_camera_obj.data.panorama_type = "EQUIRECTANGULAR"

    def __del__(self):
        #TODO: figure out if we need to remove this from the collection?
        #for collection in self._env_camera_obj.users_collection:
        #    collection.objects.unlink(self._env_camera_obj)
        bpy.data.cameras.remove(self._env_camera)

    def set_next_camera_active(self, scene, composite_manager):
        if self._current_render is RenderType.Main:
            scene.camera = self._main_camera
            scene.render.resolution_x = int(self._uncropped_res_x)
            scene.render.resolution_y = int(self._uncropped_res_y)
            composite_manager.set_main_output(self._render_output_path)

        elif self._current_render is RenderType.Probe:
            scene.camera = self._env_camera_obj
            scene.render.resolution_x = self._env_res_x
            scene.render.resolution_y = self._env_res_y
            composite_manager.set_env_output(self._render_output_path)

        self._current_render = RenderType(self._current_render.value + 1)

    def set_env_probe_location(self, navmesh_data):
        place_env_probe(self._env_camera_obj, self._main_camera, navmesh_data)

    def __lt__(self, other):
        return self._camera_index < other._camera_index


def create_view_list(cameras, output_path, scene) -> list:
    views = list()
    for index, object in enumerate(cameras):
        views.append(RenderView(object, index, output_path, scene))

    if len(views) == 0:
        print("Error: No cameras found")

    print("Found {} cameras.".format(len(views)))

    views.sort()

    return views
