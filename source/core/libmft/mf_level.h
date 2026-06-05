#pragma once

#include "level_generated.h"
#include "mf_math.h"

#include <array>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace mft
{

    enum class ImageType
    {
        DirectDiffuse,
        DirectSpecular,
        IndirectDiffuse,
        IndirectSpecular,
        Normal,
        Depth,
        Environment,
    };

    struct Level
    {
        std::vector<char> flatbuffer_data;
        std::filesystem::path mflevel_path;
        size_t blob_start_offset = 0;

        const data::Level* fbs() const
        {
            return data::GetLevel( flatbuffer_data.data() );
        }
    };

    // Load a .mflevel (or legacy .level) file. Returns false on failure.
    bool load_level( const std::string& path, Level& out );

    // BFS from start_view. Returns all view indices reachable within max_hops.
    std::unordered_set<int> get_views_in_range( const Level& level, int start_view, int max_hops );

    // Read compressed JXL bytes for a view's image into a caller-provided buffer.
    // buf must point to at least buf_size bytes. buf_size should equal ImageEntry::size().
    // Opens a fresh file stream per call — safe to call from multiple threads simultaneously.
    bool read_image_data( const Level& level, int view_index, ImageType type, void* buf, size_t buf_size );

    // Navmesh queries
    int get_view_id_from_position( const Level& level, float x, float y, float z );
    int get_navmesh_num_tris( const Level& level );
    std::array<float, 9> get_navmesh_tri_verts( const Level& level, int tri_index );

} // namespace mft
