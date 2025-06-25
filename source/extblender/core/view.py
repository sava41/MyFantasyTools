import bpy
import mathutils
import math
from enum import Enum

from .composite import *

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
    _env_camera = None
    _env_camera_obj = None
    _res_x = 1920
    _res_y = 1080
    _aspect = 1.0
    _fov = 90
    _render_output_path = ""
    _adjacent_views = {}
    
    _current_render = RenderType.Main

    def __init__(self, object, output_path):
        self._main_camera = object
        self._aspect = float(self._res_y) / float(self._res_x)
        self._fov = math.degrees(object.data.angle)
        self._render_output_path = output_path + "//renders//" + object.name

        self._main_camera.data.type = "PERSP"

        self._env_camera = bpy.data.cameras.new(object.name + "_env")
        self._env_camera_obj = bpy.data.objects.new(object.name + "_env", self._env_camera)
    
        bpy.context.scene.collection.objects.link(self._env_camera_obj)
        self._env_camera_obj.rotation_euler[0] = math.radians(90)
        self._env_camera_obj.data.type = "PANO"
        self._env_camera_obj.data.panorama_type = "EQUIRECTANGULAR"
    
    def __del__(self):
        for collection in self._env_camera_obj.users_collection:
            collection.objects.unlink(self._env_camera_obj)
        bpy.data.cameras.remove(self._env_camera)

    def set_next_camera_active(self, scene, composite_manager):
        if self._current_render is self.RenderType.Main:
            scene.camera = self._main_camera
            scene.render.resolution_x = self._res_x
            scene.render.resolution_y = self._res_y
            composite_manager.set_main_output(self._render_output_path)
        
        elif self._current_render is self.RenderType.Probe:
            scene.camera = self._env_camera_obj
            scene.render.resolution_x = 1024
            scene.render.resolution_y = 512
            composite_manager.set_env_output(self._render_output_path)

        self._current_render = self.RenderType(self._current_render.value + 1)

    # Algorithm for placing env probe
    # takes in the perimeter of the view nav mesh
    # raycasts from the main cmaera out to the furthest bounds of the perimeter
    # places env probe in the center of the raycast
    def set_env_probe_location(self, convex_hull):

        trace_start = self._main_camera.location
        trace_end = trace_start + (
            self._main_camera.matrix_world.to_quaternion()
            @ mathutils.Vector((0.0, 0.0, -10000.0))
        )

        point_close = trace_end
        point_far = trace_start

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

        self._env_camera_obj.location = (point_close + point_far) * 0.5

    def __lt__(self, other):
        return self._main_camera.data["view_id"] < other._main_camera.data["view_id"]


def create_view_list(cameras, output_path) -> list:
    views = list()
    for ob in cameras:
        views.append(View(ob.camera, output_path))

    if len(views) == 0:
        print("Error: No cameras found")

    print("Found {} cameras.".format(len(views)))

    views.sort()

    return views
