
#include "level_manager.h"

#include "io.h"
#include "jxl.h"
#include "level_generated.h"
#include "math.h"
#include "view_data.h"

#include <fstream>
#include <limits>

namespace mft
{

    LevelManager::LevelManager( ViewDataFactory factory )
        : m_viewFactory( std::move( factory ) )
    {
    }
    LevelManager::~LevelManager()
    {
        m_views.clear();
    }

    bool LevelManager::load_level( const std::string& pathString )
    {
        const std::filesystem::path levelFilePath( pathString );

        std::vector<char> dataBuffer = read_binary( levelFilePath );

        if( dataBuffer.empty() )
        {
            return false;
        }

        m_views.clear();
        m_dataBuffer = std::move( dataBuffer );

        const auto* level = data::GetLevel( reinterpret_cast<void*>( m_dataBuffer.data() ) );

        const std::filesystem::path dataFilePath = levelFilePath.parent_path() / std::filesystem::path( level->data_path()->str() );

        printf( "Data path: %s\n", level->data_path()->c_str() );

        const auto* viewsData = level->views();

        m_views.reserve( viewsData->size() );

        for( int i = 0; i < viewsData->size(); ++i )
        {
            const auto* const viewData = viewsData->Get( i );

            const auto* worldTransformPtr = viewData->world_transform();
            const std::array<float, MAT4_SIZE> transform(
                { worldTransformPtr->m00(), worldTransformPtr->m01(), worldTransformPtr->m02(), worldTransformPtr->m03(), worldTransformPtr->m10(),
                  worldTransformPtr->m11(), worldTransformPtr->m12(), worldTransformPtr->m13(), worldTransformPtr->m20(), worldTransformPtr->m21(),
                  worldTransformPtr->m22(), worldTransformPtr->m23(), worldTransformPtr->m30(), worldTransformPtr->m31(), worldTransformPtr->m32(),
                  worldTransformPtr->m33() } );

            m_views.emplace_back( m_viewFactory( viewData->name()->str(), dataFilePath, ViewData::Color | ViewData::Depth | ViewData::Environment ) );

            m_views.back()->load_camera_data( transform, viewData->res_x(), viewData->res_y(), viewData->fov() );
            m_views.back()->load_image_data();

            printf( "loaded %s\n", viewData->name()->c_str() );
        }

        return true;
    }

    int LevelManager::get_views_length() const
    {
        return m_views.size();
    }

    ViewData* LevelManager::get_view( int viewIndex )
    {
        if( 0 > viewIndex || viewIndex >= get_views_length() )
            return nullptr;

        return m_views.at( viewIndex ).get();
    }

    std::vector<int> LevelManager::get_adjacent_views( int viewIndex ) const
    {
        if( 0 > viewIndex || viewIndex >= get_views_length() )
            return {};

        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_dataBuffer.data() ) );

        const auto* adjacentViews = level->views()->Get( viewIndex )->adjacent_views();

        return std::vector<int>( adjacentViews->begin(), adjacentViews->end() );
    }

    int LevelManager::get_view_id_from_position( float x, float y, float z ) const
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_dataBuffer.data() ) );

        const auto* verts = level->navmesh_verts();

        data::Vec3 point( x, y, z );

        float minDistance = std::numeric_limits<float>::max();
        int id            = 0;

        for( auto triangle = level->navmesh_tris()->begin(); triangle != level->navmesh_tris()->end(); ++triangle )
        {
            data::Vec3 closestPoint =
                closesPointOnTriangle( *verts->Get( triangle->vert1() ), *verts->Get( triangle->vert2() ), *verts->Get( triangle->vert3() ), point );
            float distance = length( sub( point, closestPoint ) );

            if( distance < minDistance )
            {
                minDistance = distance;
                id          = triangle->view_id();
            }
        }

        return id;
    }

    int LevelManager::get_navmesh_tris_length()
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_dataBuffer.data() ) );

        return level->navmesh_tris()->size();
    }

    std::array<float, 9> LevelManager::get_navmesh_tri_verts( int triIndex )
    {
        const auto* level = data::GetLevel( reinterpret_cast<const void*>( m_dataBuffer.data() ) );

        if( triIndex < 0 || triIndex >= level->navmesh_tris()->size() )
            return { {} };

        auto triangle = level->navmesh_tris()->Get( triIndex );
        auto verts    = level->navmesh_verts();

        return { verts->Get( triangle->vert1() )->x(), verts->Get( triangle->vert1() )->y(), verts->Get( triangle->vert1() )->z(),
                 verts->Get( triangle->vert2() )->x(), verts->Get( triangle->vert2() )->y(), verts->Get( triangle->vert2() )->z(),
                 verts->Get( triangle->vert3() )->x(), verts->Get( triangle->vert3() )->y(), verts->Get( triangle->vert3() )->z() };
    }

} // namespace mft