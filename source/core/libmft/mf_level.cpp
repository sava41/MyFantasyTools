#include "mf_level.h"

#include "io.h"
#include "mf_math.h"

namespace mft
{

    bool Level::load_level( const std::string& path )
    {
        const std::filesystem::path level_file_path( path );

        m_data_buffer = read_binary( level_file_path );

        if( m_data_buffer.empty() )
        {
            return false;
        }

        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );
        m_data_file_path  = level_file_path.parent_path() / std::filesystem::path( level->data_path()->str() );
        printf( "Data path: %s\n", level->data_path()->c_str() );

        m_level_info = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );

        return true;
    }

    bool Level::load_views( std::vector<std::unique_ptr<ViewResources>>& view_resources )
    {
        if( m_level_info == nullptr )
        {
            return false;
        }

        const auto* views = m_level_info->views();

        assert( views->size() == view_resources.size() );

        for( int i = 0; i < view_resources.size(); ++i )
        {
            const auto* const view_info = views->Get( i );

            view_resources.at( i )->init( view_info, m_data_file_path );

            view_resources.at( i )->load_image_data();
            view_resources.at( i )->load_camera_data();

            printf( "loaded %s\n", view_info->name()->c_str() );
        }

        return true;
    }

    std::vector<int> Level::get_adjacent_views( int view_index ) const
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );

        assert( view_index >= 0 && view_index < level->views()->size() );

        const auto* adjacent_views = level->views()->Get( view_index )->adjacent_views();

        return std::vector<int>( adjacent_views->begin(), adjacent_views->end() );
    }

    int Level::get_view_id_from_position( float x, float y, float z ) const
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );

        const auto* verts = level->navmesh_verts();

        data::Vec3 point( x, y, z );

        float min_distance = std::numeric_limits<float>::max();
        int id             = 0;

        for( auto triangle = level->navmesh_tris()->begin(); triangle != level->navmesh_tris()->end(); ++triangle )
        {
            data::Vec3 closest_point =
                closesPointOnTriangle( *verts->Get( triangle->vert1() ), *verts->Get( triangle->vert2() ), *verts->Get( triangle->vert3() ), point );
            float distance = length( sub( point, closest_point ) );

            if( distance < min_distance )
            {
                min_distance = distance;
                id           = triangle->view_id();
            }
        }

        return id;
    }

    int Level::get_num_views() const
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );

        const auto* views_data = level->views();

        return views_data->size();
    }

    int Level::get_navmesh_num_tris()
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );

        return level->navmesh_tris()->size();
    }

    std::array<float, 9> Level::get_navmesh_tri_verts( int tri_index )
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_data_buffer.data() ) );

        assert( tri_index >= 0 && tri_index < level->navmesh_tris()->size() );

        auto triangle = level->navmesh_tris()->Get( tri_index );
        auto verts    = level->navmesh_verts();

        return { verts->Get( triangle->vert1() )->x(), verts->Get( triangle->vert1() )->y(), verts->Get( triangle->vert1() )->z(),
                 verts->Get( triangle->vert2() )->x(), verts->Get( triangle->vert2() )->y(), verts->Get( triangle->vert2() )->z(),
                 verts->Get( triangle->vert3() )->x(), verts->Get( triangle->vert3() )->y(), verts->Get( triangle->vert3() )->z() };
    }


} // namespace mft
