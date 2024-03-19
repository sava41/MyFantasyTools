import bpy
from math import degrees
from mathutils import Euler


class Camera:
    def __init__(self, object, output_path):
        self.object = object
        self.res_x = 1920
        self.res_y = 1080
        self.aspect = float(self.res_y) / float(self.res_x)
        self.render_output_path = output_path + "//renders//" + object.name
        self.adjacent_views = {}

        self.fov = degrees(object.data.angle)

    def set_active(self, scene):
        scene.camera = self.object
        self.object.data.type = "PERSP"
        scene.render.resolution_x = self.res_x
        scene.render.resolution_y = self.res_y

    def set_env_probe_active(self, scene):
        scene.camera = self.object
        self.object.data.type = "PANO"
        self.object.data.panorama_type = "EQUIRECTANGULAR"
        scene.render.resolution_x = 1024
        scene.render.resolution_y = 512

        self.object.rotation_euler = Euler((90.0, 0.0, 0.0), "XYZ")

    def __lt__(self, other):
        return self.object.data["view_id"] < other.object.data["view_id"]


def create_camera_list(scene, output_path) -> list:
    cameras = list()
    for ob in scene.objects:
        if ob.type == "CAMERA":
            cameras.append(Camera(ob, output_path))

    if len(cameras) == 0:
        print("Error: No cameras found")

    print("Found {} cameras.".format(len(cameras)))

    cameras.sort()

    return cameras
