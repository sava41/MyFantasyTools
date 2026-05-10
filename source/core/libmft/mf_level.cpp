#include "mf_level.h"

#include "io.h"

#include <cassert>
#include <fstream>
#include <limits>
#include <queue>

namespace mft
{

    static const data::ImageEntry* get_image_entry( const data::View* view, ImageType type )
    {
        switch( type )
        {
        case ImageType::ColorDirect:
            return view->color_direct();
        case ImageType::ColorIndirect:
            return view->color_indirect();
        case ImageType::Depth:
            return view->depth();
        case ImageType::Environment:
            return view->environment();
        case ImageType::LightDirection:
            return view->light_direction();
        }
        return nullptr;
    }

    bool load_level( const std::string& path, Level& out )
    {
        const std::filesystem::path level_path( path );

        if( !read_mflevel( level_path, out.flatbuffer_data, out.blob_start_offset ) )
            return false;

        out.mflevel_path = level_path;
        return true;
    }

    std::unordered_set<int> get_views_in_range( const Level& level, int start_view, int max_hops )
    {
        const auto* views = level.fbs()->views();

        std::unordered_set<int> visited;
        std::queue<std::pair<int, int>> queue; // {view_index, depth}
        queue.push( { start_view, 0 } );
        visited.insert( start_view );

        while( !queue.empty() )
        {
            auto [idx, depth] = queue.front();
            queue.pop();

            if( depth >= max_hops )
                continue;

            for( auto adj : *views->Get( idx )->adjacent_views() )
            {
                if( !visited.count( adj ) )
                {
                    visited.insert( adj );
                    queue.push( { adj, depth + 1 } );
                }
            }
        }

        return visited;
    }

    bool read_image_data( const Level& level, int view_index, ImageType type, void* buf, size_t buf_size )
    {
        const auto* view  = level.fbs()->views()->Get( view_index );
        const auto* entry = get_image_entry( view, type );

        if( !entry )
            return false;

        std::ifstream file( level.mflevel_path, std::ios::binary );
        if( !file.good() )
        {
            fprintf( stderr, "Could not open %s\n", level.mflevel_path.string().c_str() );
            return false;
        }

        file.seekg( static_cast<std::streamoff>( level.blob_start_offset + entry->offset() ) );
        if( !file.good() )
        {
            fprintf( stderr, "Seek failed for view %d\n", view_index );
            return false;
        }

        file.read( static_cast<char*>( buf ), static_cast<std::streamsize>( buf_size ) );
        if( !file.good() )
        {
            fprintf( stderr, "Read failed for view %d\n", view_index );
            return false;
        }

        return true;
    }

    int get_view_id_from_position( const Level& level, float x, float y, float z )
    {
        const auto* fbs   = level.fbs();
        const auto* verts = fbs->navmesh_verts();
        data::Vec3 point( x, y, z );

        float min_distance = std::numeric_limits<float>::max();
        int id             = 0;

        for( auto tri = fbs->navmesh_tris()->begin(); tri != fbs->navmesh_tris()->end(); ++tri )
        {
            data::Vec3 closest = closesPointOnTriangle( *verts->Get( tri->vert1() ), *verts->Get( tri->vert2() ), *verts->Get( tri->vert3() ), point );
            float dist         = length( sub( point, closest ) );
            if( dist < min_distance )
            {
                min_distance = dist;
                id           = tri->view_id();
            }
        }

        return id;
    }

    int get_navmesh_num_tris( const Level& level )
    {
        return static_cast<int>( level.fbs()->navmesh_tris()->size() );
    }

    std::array<float, 9> get_navmesh_tri_verts( const Level& level, int tri_index )
    {
        const auto* fbs   = level.fbs();
        const auto* tri   = fbs->navmesh_tris()->Get( tri_index );
        const auto* verts = fbs->navmesh_verts();
        assert( tri_index >= 0 && tri_index < (int)fbs->navmesh_tris()->size() );
        return { verts->Get( tri->vert1() )->x(), verts->Get( tri->vert1() )->y(), verts->Get( tri->vert1() )->z(),
                 verts->Get( tri->vert2() )->x(), verts->Get( tri->vert2() )->y(), verts->Get( tri->vert2() )->z(),
                 verts->Get( tri->vert3() )->x(), verts->Get( tri->vert3() )->y(), verts->Get( tri->vert3() )->z() };
    }

} // namespace mft
