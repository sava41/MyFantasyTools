import bpy
import bmesh
import mathutils

from . color import CAMERA_COLOR_ATTR, color_to_comparable, colors_match, face_color


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

    def find_adjacent_views(self, camera_color, view_index_map) -> list:
        """
        Find adjacent views by camera color.

        Args:
            camera_color: RGB tuple of the camera color to find adjacencies for
            view_index_map: Dictionary mapping camera colors to view indices

        Returns:
            List of adjacent view indices
        """
        bm = bmesh.new()
        bm.from_mesh(self.object.data)

        color_layer = bm.loops.layers.float_color.get(CAMERA_COLOR_ATTR)
        if not color_layer:
            bm.free()
            return []

        adj_views = list()

        # Find all faces matching the current camera color
        current_view_faces = [
            f for f in bm.faces
            if colors_match(face_color(f, color_layer), camera_color)
        ]

        # Find adjacent faces with different colors
        for f in current_view_faces:
            for edge in f.edges:
                for adj_face in edge.link_faces:
                    adj_color = color_to_comparable(face_color(adj_face, color_layer))
                    if not colors_match(face_color(adj_face, color_layer), camera_color):
                        if adj_color in view_index_map:
                            adj_views.append(view_index_map[adj_color])

            # Check vertex-edge-vertex adjacency
            for vert in f.verts:
                for edge in vert.link_edges:
                    other_vert = edge.other_vert(vert)
                    if other_vert not in f.verts:
                        for other_face in other_vert.link_faces:
                            adj_color = color_to_comparable(face_color(other_face, color_layer))
                            if not colors_match(face_color(other_face, color_layer), camera_color):
                                if adj_color in view_index_map:
                                    adj_views.append(view_index_map[adj_color])

        bm.free()

        return list(set(adj_views))

    def get_view_color_tris(self, camera_color) -> tuple:
        """
        Get triangles and convex hull for faces matching a camera color.

        Args:
            camera_color: RGB tuple of the camera color

        Returns:
            Tuple of (triangles list, convex_hull_2d list)
        """
        bm = bmesh.new()
        bm.from_mesh(self.object.data)
        bm.faces.ensure_lookup_table()

        color_layer = bm.loops.layers.float_color.get(CAMERA_COLOR_ATTR)
        if not color_layer:
            bm.free()
            return [], []

        verts = list()
        triangles = list()

        # Triangulate the bmesh
        bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.faces.ensure_lookup_table()

        for f in bm.faces:
            if colors_match(face_color(f, color_layer), camera_color):
                # Create a simple object to hold triangle vertex coordinates
                tri_verts = [vert.co.copy() for vert in f.verts]
                tri_obj = type('Triangle', (), {'verts': tri_verts})()
                triangles.append(tri_obj)
                for vert in f.verts:
                    verts.append([vert.co.x, vert.co.y])

        if len(verts) == 0:
            bm.free()
            return triangles, []

        vert_indicies = mathutils.geometry.convex_hull_2d(verts)

        bm.free()

        convex_hull_2d = [verts[i] for i in vert_indicies if i < len(verts)]

        return triangles, convex_hull_2d
