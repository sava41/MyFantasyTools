import bpy
import bmesh
import mathutils


class Navmesh:
    def __init__(self, object):
        self.object = object

        self.object.data.calc_loop_triangles()

    def select_edit_navmesh(self):
        bpy.ops.object.select_all(action="DESELECT")
        self.object.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")

    def deselect_navmesh(self):
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")

    def find_adjacent_views(self, view_id) -> list:
        bm = bmesh.new()
        bm.from_mesh(self.object.data)
        view_attribute = bm.faces.layers.int["Views"]

        adj_views = list()
        for face in bm.faces:
            if face[view_attribute] is view_id:
                for edge in face.edges:
                    for adj_face in edge.link_faces:
                        if adj_face[view_attribute] is not view_id:
                            adj_views.append(adj_face[view_attribute])

        bm.free()

        return set(adj_views)

    def find_convex_hull_area(self, view_id) -> list:
        bm = bmesh.new()
        bm.from_mesh(self.object.data)
        view_attribute = bm.faces.layers.int["Views"]

        verts = list()
        for face in bm.faces:
            if face[view_attribute] is view_id:
                for vert in face.verts:
                    verts.append([vert.co.x, vert.co.y])

        vert_indicies = mathutils.geometry.convex_hull_2d(verts)

        bm.free()

        return [verts[i] for i in vert_indicies if i < len(verts)]
