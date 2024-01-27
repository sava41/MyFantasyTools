import bpy
import bmesh


class Navmesh:
    def __init__(self, object):
        self.object = object
        self.name = object.name.removeprefix("navmesh_")

        self.object.data.calc_loop_triangles()

    def find_adjacent_views(self, view_id) -> list:
        bpy.ops.object.select_all(action="DESELECT")
        self.object.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")

        # for i, polygon in enumerate(self.object.data.polygons):
        #    self.object.data.polygons[i].select = view_data[i].value == view_id

        bm = bmesh.from_edit_mesh(self.object.data)

        view_attribute = bm.faces.layers.int["Views"]

        adj_views = list()
        for face in bm.faces:
            if face[view_attribute] is view_id:
                for edge in face.edges:
                    for adj_face in edge.link_faces:
                        if adj_face[view_attribute] is not view_id:
                            adj_views.append(adj_face[view_attribute])

        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")

        return set(adj_views)


def create_navmesh_list(scene) -> list:
    navmeshes = list()
    for ob in scene.objects:
        if (
            ob.type == "MESH"
            and ob.name.startswith("navmesh_")
            and "Views" in ob.data.attributes.keys()
        ):
            navmeshes.append(Navmesh(ob))

    if len(navmeshes) == 0:
        print("Error: No navmeshes found")

    return navmeshes
