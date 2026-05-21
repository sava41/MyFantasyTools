#include "gd_manager.h"

#include "jxl.h"

#include <fstream>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <thread>

MFManager::MFManager()
    : godot::Object()
{
    m_static_inst = this;
}

MFManager::~MFManager()
{
    m_static_inst = nullptr;
}

bool MFManager::load( const godot::String& path )
{
    if( m_active_path == path )
        return true;

    for( const auto& status : m_view_cache_status )
        while( status.load( std::memory_order_acquire ) == ViewStatus::loading )
            ;

    const godot::String full_path = godot::ProjectSettings::get_singleton()->globalize_path( path );

    if( !mft::load_level( full_path.utf8().get_data(), m_level ) )
    {
        godot::UtilityFunctions::print( "MFManager: failed to load " + full_path );
        return false;
    }

    const auto num_views = static_cast<size_t>( get_num_views() );
    m_view_cache.clear();
    m_view_cache.resize( num_views );
    m_view_cache_status = std::vector<std::atomic<ViewStatus>>( num_views );

    m_active_path     = path;
    m_current_view_id = -1;

    return true;
}

godot::String MFManager::get_active_path() const
{
    return m_active_path;
}

int MFManager::get_num_views() const
{
    if( !m_level.fbs() || !m_level.fbs()->views() )
        return 0;
    return static_cast<int>( m_level.fbs()->views()->size() );
}

int MFManager::get_closest_view_id( const godot::Vector3& point ) const
{
    // Godot uses y-up; convert to z-up for mft.
    const godot::Vector3 rotated = point.rotated( godot::Vector3( 1, 0, 0 ), Math_PI * 0.5 );
    return mft::get_view_id_from_position( m_level, rotated.x, rotated.y, rotated.z );
}

godot::PackedVector3Array MFManager::get_navmesh()
{
    const int num_tris = mft::get_navmesh_num_tris( m_level );

    godot::PackedVector3Array tris;
    tris.resize( num_tris * 3 );

    for( int i = 0; i < num_tris; ++i )
    {
        const auto verts = mft::get_navmesh_tri_verts( m_level, i );

        // Godot uses CW winding order while mft uses CCW, so insert verts in reverse order.
        // Input transform is z-up; convert to y-up for Godot.
        tris.set( ( i * 3 ) + 2, godot::Vector3( verts[0], verts[1], verts[2] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
        tris.set( ( i * 3 ) + 1, godot::Vector3( verts[3], verts[4], verts[5] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
        tris.set( ( i * 3 ) + 0, godot::Vector3( verts[6], verts[7], verts[8] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
    }

    return tris;
}

bool MFManager::set_current_view( int view_id )
{
    if( m_active_path.is_empty() )
        return false;

    const int num_views = get_num_views();
    if( view_id < 0 || view_id >= num_views || view_id == m_current_view_id )
        return false;

    m_current_view_id = view_id;

    constexpr int k_max_hops = 2;
    m_in_range               = mft::get_views_in_range( m_level, view_id, k_max_hops );

    for( int view : m_in_range )
    {
        if( m_view_cache_status[view].load( std::memory_order_relaxed ) == ViewStatus::unloaded )
        {
            begin_load_view( view );
        }
    }

    emit_signal( "current_view_changed", m_active_path, view_id );

    return true;
}

static godot::Image::Format channels_to_format( uint8_t channels )
{
    switch( channels )
    {
    case 1:
        return godot::Image::FORMAT_RF;
    case 3:
        return godot::Image::FORMAT_RGBF;
    default:
        return godot::Image::FORMAT_RGBF;
    }
}

void MFManager::begin_load_view( int view_index )
{
    const auto* view = m_level.fbs()->views()->Get( view_index );

    const auto* cd  = view->color_direct();
    const auto* ci  = view->color_indirect();
    const auto* dep = view->depth();
    const auto* env = view->environment();
    const auto* ld  = view->light_direction();

    // Allocate Images on the main thread; the worker writes into their raw buffers.
    auto cache            = std::make_unique<ViewCache>();
    cache->color_direct   = godot::Image::create( cd->res_x(), cd->res_y(), false, channels_to_format( cd->channels() ) );
    cache->color_indirect = godot::Image::create( ci->res_x(), ci->res_y(), false, channels_to_format( ci->channels() ) );
    cache->depth          = godot::Image::create( dep->res_x(), dep->res_y(), false, channels_to_format( dep->channels() ) );
    cache->env            = godot::Image::create( env->res_x(), env->res_y(), false, channels_to_format( env->channels() ) );
    cache->light_dir      = godot::Image::create( ld->res_x(), ld->res_y(), false, channels_to_format( ld->channels() ) );

    // Pre-extract all file offsets and pixel sizes before dispatching — avoids any
    // cross-thread access to the flatbuffer or the Level struct.
    const size_t blob = m_level.blob_start_offset;

    struct ImageLoad
    {
        size_t file_offset;
        size_t compressed_size;
        void* out_buf;
        size_t pixel_bytes;
    };

    const std::vector<ImageLoad> loads = {
        { blob + static_cast<size_t>( cd->offset() ), static_cast<size_t>( cd->size() ), const_cast<uint8_t*>( cache->color_direct->get_data().ptr() ),
          static_cast<size_t>( cd->res_x() ) * cd->res_y() * cd->channels() * sizeof( float ) },
        { blob + static_cast<size_t>( ci->offset() ), static_cast<size_t>( ci->size() ), const_cast<uint8_t*>( cache->color_indirect->get_data().ptr() ),
          static_cast<size_t>( ci->res_x() ) * ci->res_y() * ci->channels() * sizeof( float ) },
        { blob + static_cast<size_t>( dep->offset() ), static_cast<size_t>( dep->size() ), const_cast<uint8_t*>( cache->depth->get_data().ptr() ),
          static_cast<size_t>( dep->res_x() ) * dep->res_y() * dep->channels() * sizeof( float ) },
        { blob + static_cast<size_t>( env->offset() ), static_cast<size_t>( env->size() ), const_cast<uint8_t*>( cache->env->get_data().ptr() ),
          static_cast<size_t>( env->res_x() ) * env->res_y() * env->channels() * sizeof( float ) },
        { blob + static_cast<size_t>( ld->offset() ), static_cast<size_t>( ld->size() ), const_cast<uint8_t*>( cache->light_dir->get_data().ptr() ),
          static_cast<size_t>( ld->res_x() ) * ld->res_y() * ld->channels() * sizeof( float ) },
    };

    m_view_cache_status[view_index].store( ViewStatus::loading, std::memory_order_relaxed );
    m_view_cache[view_index] = std::move( cache );

    std::atomic<ViewStatus>* status = &m_view_cache_status[view_index];

    std::thread(
        [status, loads, file_path = m_level.mflevel_path]()
        {
            std::ifstream file( file_path, std::ios::binary );
            if( !file )
            {
                status->store( ViewStatus::ready, std::memory_order_release );
                return;
            }

            for( const auto& img : loads )
            {
                std::vector<char> compressed( img.compressed_size );
                file.seekg( static_cast<std::streamoff>( img.file_offset ) );
                file.read( compressed.data(), static_cast<std::streamsize>( img.compressed_size ) );
                if( !file )
                    break;
                if( !mft::jxl::decode_jxl( compressed.data(), img.compressed_size, img.out_buf, img.pixel_bytes ) )
                    break;
            }

            status->store( ViewStatus::ready, std::memory_order_release );
        } )
        .detach();
}

void MFManager::poll_pending()
{
    const int num_views = get_num_views();

    for( int view_id = 0; view_id < num_views; ++view_id )
    {
        const ViewStatus s = m_view_cache_status[view_id].load( std::memory_order_acquire );

        if( m_in_range.count( view_id ) > 0 && s == ViewStatus::ready )
        {
            m_view_cache_status[view_id].store( ViewStatus::loaded, std::memory_order_relaxed );
            emit_signal( "view_data_ready", m_active_path, view_id );
        }
        else if( s == ViewStatus::loaded || s == ViewStatus::ready )
        {
            m_view_cache[view_id].reset();
            m_view_cache_status[view_id].store( ViewStatus::unloaded, std::memory_order_relaxed );
        }
    }
}

bool MFManager::is_view_loaded( int view_id ) const
{
    return m_view_cache_status[view_id].load( std::memory_order_relaxed ) == ViewStatus::loaded;
}

const MFManager::ViewCache* MFManager::get_view_cache( int view_id ) const
{
    if( m_view_cache_status[view_id].load( std::memory_order_relaxed ) != ViewStatus::loaded )
        return nullptr;
    return m_view_cache[view_id].get();
}

void MFManager::_bind_methods()
{
    ADD_SIGNAL( godot::MethodInfo( "current_view_changed", godot::PropertyInfo( godot::Variant::STRING, "path" ),
                                   godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );
    ADD_SIGNAL(
        godot::MethodInfo( "view_data_ready", godot::PropertyInfo( godot::Variant::STRING, "path" ), godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );

    godot::ClassDB::bind_method( godot::D_METHOD( "load", "path" ), &MFManager::load );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_active_path" ), &MFManager::get_active_path );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_num_views" ), &MFManager::get_num_views );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_closest_view_id", "point" ), &MFManager::get_closest_view_id );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_current_view", "view_id" ), &MFManager::set_current_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_navmesh" ), &MFManager::get_navmesh );
    godot::ClassDB::bind_method( godot::D_METHOD( "is_view_loaded", "view_id" ), &MFManager::is_view_loaded );
}
