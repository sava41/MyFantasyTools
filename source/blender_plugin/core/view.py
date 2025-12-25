import bpy
import mathutils
import math
from enum import Enum

from .composite import *

#TODO: should to merge this and the MFT_Camera class at some point
class View:
    class RenderType(Enum):
        Main = 1
        Probe = 2
        Complete = 3

        def next(self):
          members = list(self.__class__)
          index = members.index(self)
          return members[index + 1] if index + 1 < len(members) else None

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
    
    _current_render = RenderType.Main

    def __init__(self, object, output_path, scene):

        self._res_x = scene.mft_global_settings.render_width
        self._res_y = scene.mft_global_settings.render_height
        
        self._max_pan = object.max_pan
        self._max_tilt = object.max_tilt
        
        camera = object.camera

        self._name = camera.name
        self._main_camera = camera.copy()
        self._main_camera.data = camera.data.copy()
        self._aspect = float(self._res_y) / float(self._res_x)
        
        #horizontal fov
        self._fov = camera.data.angle
        self._render_output_path = output_path + "//renders//" + camera.name

        # Compute actual FOV needed by checking rotated frustum corners
        hfov_half = camera.data.angle * 0.5
        vfov = 2 * math.atan( math.tan(camera.data.angle * 0.5) * self._aspect )
        vfov_half = vfov * 0.5

        # Check all 4 corners at max rotation to find required FOV
        max_angle_h = 0
        max_angle_v = 0

        for corner_h in [-hfov_half, hfov_half]:
            for corner_v in [-vfov_half, vfov_half]:
                # Create corner ray direction
                corner_dir = mathutils.Vector((
                    math.sin(corner_h) * math.cos(corner_v),
                    math.sin(corner_v),
                    -math.cos(corner_h) * math.cos(corner_v)
                ))

                # Rotate by max pan (around Y axis)
                rot_pan = mathutils.Matrix.Rotation(object.max_pan, 3, 'Y')
                # Rotate by max tilt (around X axis)
                rot_tilt = mathutils.Matrix.Rotation(object.max_tilt, 3, 'X')

                # Apply rotations (order matters: tilt then pan)
                rotated_dir = rot_pan @ rot_tilt @ corner_dir

                # Compute angles from optical axis
                horizontal_dist = math.sqrt(rotated_dir.x**2 + rotated_dir.z**2)
                angle_h = abs(math.atan2(rotated_dir.x, -rotated_dir.z))
                angle_v = abs(math.atan2(rotated_dir.y, horizontal_dist))

                max_angle_h = max(max_angle_h, angle_h)
                max_angle_v = max(max_angle_v, angle_v)

        self._uncropped_fov = max_angle_h * 2.0
        uncropped_vfov = max_angle_v * 2.0

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
        if self._current_render is self.RenderType.Main:
            scene.camera = self._main_camera
            scene.render.resolution_x = int(self._uncropped_res_x)
            scene.render.resolution_y = int(self._uncropped_res_y)
            composite_manager.set_main_output(self._render_output_path)
        
        elif self._current_render is self.RenderType.Probe:
            scene.camera = self._env_camera_obj
            scene.render.resolution_x = 1024
            scene.render.resolution_y = 512
            composite_manager.set_env_output(self._render_output_path)

        self._current_render = self.RenderType(self._current_render.value + 1)

    # Algorithm for placing env probe
    # takes in the perimeter of the view nav mesh
    # raycasts from the main camera out to the furthest bounds of the perimeter
    # places env probe in the center of the raycast
    def set_env_probe_location(self, navmesh_data):

        tris, convex_hull, = navmesh_data

        trace_dir = self._main_camera.matrix_world.to_quaternion() @ mathutils.Vector((0.0, 0.0, -10000.0))
        trace_start = self._main_camera.location
        trace_end = trace_start +  trace_dir
        
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

        self._env_camera_obj.location = (point_close + point_far) * 0.5

    def __lt__(self, other):
        return self._main_camera.data["view_id"] < other._main_camera.data["view_id"]


def create_view_list(cameras, output_path, scene) -> list:
    views = list()
    for object in cameras:
        views.append(View(object, output_path, scene))

    if len(views) == 0:
        print("Error: No cameras found")

    print("Found {} cameras.".format(len(views)))

    views.sort()

    return views
