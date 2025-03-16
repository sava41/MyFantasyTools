import bpy
import mathutils
import math


class View:
    def __init__(self, object, output_path):
        self.main_camera = object
        self.res_x = 1920
        self.res_y = 1080
        self.aspect = float(self.res_y) / float(self.res_x)
        self.fov = math.degrees(object.data.angle)
        self.render_output_path = output_path + "//renders//" + object.name
        self.adjacent_views = {}

        self.main_camera.data.type = "PERSP"

        self.env_camera = bpy.data.objects.new(
            object.name + "_env", bpy.data.cameras.new(object.name + "_env")
        )
        bpy.context.scene.collection.objects.link(self.env_camera)
        self.env_camera.rotation_euler[0] = math.radians(90)
        self.env_camera.data.type = "PANO"
        self.env_camera.data.panorama_type = "EQUIRECTANGULAR"

    def set_active(self, scene):
        scene.camera = self.main_camera
        scene.render.resolution_x = self.res_x
        scene.render.resolution_y = self.res_y

    def set_env_probe_active(self, scene):
        scene.camera = self.env_camera
        scene.render.resolution_x = 1024
        scene.render.resolution_y = 512

    # Algorithm for placing env probe
    # takes in the perimeter of the view nav mesh
    # raycasts from the main cmaera out to the furthest bounds of the perimeter
    # places env probe in the center of the raycast
    def set_env_probe_location(self, convex_hull):

        trace_start = self.main_camera.location
        trace_end = trace_start + (
            self.main_camera.matrix_world.to_quaternion()
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

        self.env_camera.location = (point_close + point_far) * 0.5

    def __lt__(self, other):
        return self.main_camera.data["view_id"] < other.main_camera.data["view_id"]


def create_view_list(scene, output_path) -> list:
    views = list()
    for ob in scene.objects:
        if ob.type == "CAMERA":
            views.append(View(ob, output_path))

    if len(views) == 0:
        print("Error: No cameras found")

    print("Found {} cameras.".format(len(views)))

    views.sort()

    return views
