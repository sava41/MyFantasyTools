#pragma once

#include "level_generated.h"
#include "mf_view_resources.h"

#include <filesystem>
#include <string>
#include <vector>

namespace mft
{
    class Level
    {
      public:
        Level()  = default;
        ~Level() = default;

        bool load_level( const std::string& path );

        int get_num_views() const;
        int get_view_id_from_position( float x, float y, float z ) const;
        std::vector<int> get_adjacent_views( int viewIndex ) const;

        int get_navmesh_num_tris();
        std::array<float, 9> get_navmesh_tri_verts( int triIndex );

      protected:
        bool load_views( std::vector<std::unique_ptr<ViewResources>>& view_resources );

        std::vector<char> m_data_buffer;
        const mft::data::Level* m_level_info = nullptr;

        std::filesystem::path m_data_file_path;   // set for legacy .level format
        std::filesystem::path m_mflevel_path;     // set when loading from .mflevel
        size_t                m_blob_start_offset = 0; // byte offset of image blob in .mflevel
    };

} // namespace mft
