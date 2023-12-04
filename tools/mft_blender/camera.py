import bpy


class Camera:
    def __init__(self, object, output_path):
        self.object = object
        self.res_x = 1920
        self.res_y = 1080
        self.aspect = float(self.res_y) / float(self.res_x)
        self.output_path = output_path + "//" + object.name
        self.adjacent_views = {}

    def set_active(self, scene):
        scene.camera = self.object
        scene.render.resolution_x = self.res_x
        scene.render.resolution_y = self.res_y

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
