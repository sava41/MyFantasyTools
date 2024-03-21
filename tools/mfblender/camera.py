import bpy
import mathutils
import math


class Camera:
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

    def set_env_probe_location(self, location):
        self.env_camera.location = location

    def __lt__(self, other):
        return self.main_camera.data["view_id"] < other.main_camera.data["view_id"]


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
