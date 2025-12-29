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

        current_view_faces = [face for face in bm.faces if face[view_attribute] is view_id]

        for face in current_view_faces:
            for edge in face.edges:
                for adj_face in edge.link_faces:
                    if adj_face[view_attribute] is not view_id:
                        adj_views.append(adj_face[view_attribute])

            # Check vertex-edge-vertex adjacency
            for vert in face.verts:
                for edge in vert.link_edges:
                    other_vert = edge.other_vert(vert)
                    if other_vert not in face.verts:
                        for other_face in other_vert.link_faces:
                            if other_face[view_attribute] is not view_id:
                                adj_views.append(other_face[view_attribute])

        bm.free()

        return set(adj_views)

    def get_view_id_tris(self, view_id) -> tuple:
        bm = bmesh.new()
        bm.from_mesh(self.object.data)
        bm.faces.ensure_lookup_table()
        view_attribute = bm.faces.layers.int["Views"]

        verts = list()
        triangles = list()
        # Triangulate the bmesh
        bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.faces.ensure_lookup_table()

        for face in bm.faces:
            if face[view_attribute] is view_id:
                # Create a simple object to hold triangle vertex coordinates
                tri_verts = [vert.co.copy() for vert in face.verts]
                tri_obj = type('Triangle', (), {'verts': tri_verts})()
                triangles.append(tri_obj)
                for vert in face.verts:
                    verts.append([vert.co.x, vert.co.y])

        vert_indicies = mathutils.geometry.convex_hull_2d(verts)

        bm.free()

        convex_hull_2d = [verts[i] for i in vert_indicies if i < len(verts)]

        return triangles, convex_hull_2d
