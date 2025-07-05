
#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mft
{

    class ViewData;

    using ViewDataFactory = std::function<std::unique_ptr<ViewData>( const std::string&, const std::filesystem::path&, unsigned int )>;

    class LevelManager
    {
      public:
        LevelManager( ViewDataFactory factory );
        ~LevelManager();

        bool load_level( const std::string& path );
        int get_views_length() const;

        ViewData* get_view( int viewIndex );

        std::vector<int> get_adjacent_views( int viewIndex ) const;

        int get_view_id_from_position( float x, float y, float z ) const;

        int get_navmesh_tris_length();
        std::array<float, 9> get_navmesh_tri_verts( int triIndex );

      private:
        std::vector<char> m_dataBuffer;
        std::vector<std::unique_ptr<ViewData>> m_views;
        ViewDataFactory m_viewFactory;
    };

} // namespace mft
