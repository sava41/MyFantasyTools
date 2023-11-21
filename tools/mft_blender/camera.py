import bpy


class Camera:
    def __init__(self, object, id, output_path):
        self.object = object
        self.id = id
        self.res_x = 1920
        self.res_y = 1080
        self.aspect = float(self.res_y) / float(self.res_x)
        self.output_path = output_path + "//" + object.name

    def set_active(self, scene):
        scene.camera = self.object
        scene.render.resolution_x = self.res_x
        scene.render.resolution_y = self.res_y

    def get_output_path(self) -> str:
        return self.output_path


def create_camera_list(scene, output_path) -> list:
    cameras = list()
    index = 0
    for ob in scene.objects:
        if ob.type == "CAMERA":
            cameras.append(Camera(ob, index, output_path))
            index += 1

    if index == 0:
        print("Error: No cameras found")

    return cameras
