import bpy
import bmesh
import mathutils


def color_to_comparable(color_rgba):
    """
    Convert RGBA color tuple to a comparable format for equality checks.
    Rounds to 3 decimal places to avoid floating point comparison issues.
    """
    return tuple(round(c, 3) for c in color_rgba[:3])


def colors_match(color1, color2, tolerance=0.01):
    """
    Check if two colors match within a tolerance.
    """
    if len(color1) < 3 or len(color2) < 3:
        return False
    return all(abs(color1[i] - color2[i]) < tolerance for i in range(3))


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

        # Get the color attribute layer
        color_layer = bm.faces.layers.color.get("CameraColor")
        if not color_layer:
            bm.free()
            return []

        adj_views = list()

        # Find all faces matching the current camera color
        current_view_faces = [
            face for face in bm.faces
            if colors_match(face[color_layer], camera_color)
        ]

        # Find adjacent faces with different colors
        for face in current_view_faces:
            for edge in face.edges:
                for adj_face in edge.link_faces:
                    adj_color = color_to_comparable(adj_face[color_layer])
                    if not colors_match(adj_face[color_layer], camera_color):
                        if adj_color in view_index_map:
                            adj_views.append(view_index_map[adj_color])

            # Check vertex-edge-vertex adjacency
            for vert in face.verts:
                for edge in vert.link_edges:
                    other_vert = edge.other_vert(vert)
                    if other_vert not in face.verts:
                        for other_face in other_vert.link_faces:
                            adj_color = color_to_comparable(other_face[color_layer])
                            if not colors_match(other_face[color_layer], camera_color):
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

        # Get the color attribute layer
        color_layer = bm.faces.layers.color.get("CameraColor")
        if not color_layer:
            bm.free()
            return [], []

        verts = list()
        triangles = list()

        # Triangulate the bmesh
        bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.faces.ensure_lookup_table()

        for face in bm.faces:
            if colors_match(face[color_layer], camera_color):
                # Create a simple object to hold triangle vertex coordinates
                tri_verts = [vert.co.copy() for vert in face.verts]
                tri_obj = type('Triangle', (), {'verts': tri_verts})()
                triangles.append(tri_obj)
                for vert in face.verts:
                    verts.append([vert.co.x, vert.co.y])

        if len(verts) == 0:
            bm.free()
            return triangles, []

        vert_indicies = mathutils.geometry.convex_hull_2d(verts)

        bm.free()

        convex_hull_2d = [verts[i] for i in vert_indicies if i < len(verts)]

        return triangles, convex_hull_2d
