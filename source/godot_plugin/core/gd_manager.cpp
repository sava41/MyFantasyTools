#include "gd_manager.h"

#include "jxl.h"

#include <fstream>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <thread>

static godot::Transform3D view_transform( const mft::data::Mat4* mat )
{
    // Flatbuffer Mat4 stores column-major; rows 0-2 are basis, column 3 is translation.
    godot::Transform3D t( mat->m00(), mat->m01(), mat->m02(), mat->m10(), mat->m11(), mat->m12(), mat->m20(), mat->m21(), mat->m22(), mat->m03(), mat->m13(),
                          mat->m23() );

    // Input transform is z-up; convert to y-up for Godot.
    t.rotate( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 );
    return t;
}

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

    const godot::String full_path = godot::ProjectSettings::get_singleton()->globalize_path( path );

    m_loaded.clear();
    m_pending.clear();

    if( !mft::load_level( full_path.utf8().get_data(), m_level ) )
    {
        godot::UtilityFunctions::print( "MFManager: failed to load " + full_path );
        return false;
    }

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
    const auto in_range      = mft::get_views_in_range( m_level, view_id, k_max_hops );

    // Evict loaded views that are no longer in range.
    for( auto it = m_loaded.begin(); it != m_loaded.end(); )
    {
        if( in_range.find( it->first ) == in_range.end() )
        {
            it = m_loaded.erase( it );
        }
        else
        {
            ++it;
        }
    }

    // Drop pending loads that are no longer in range.
    for( auto it = m_pending.begin(); it != m_pending.end(); )
    {
        if( in_range.find( it->first ) == in_range.end() )
        {
            it = m_pending.erase( it );
        }
        else
        {
            ++it;
        }
    }

    // Begin loading views in range that are not yet loaded or pending.
    for( int view : in_range )
    {
        if( m_loaded.find( view ) == m_loaded.end() && m_pending.find( view ) == m_pending.end() )
        {
            begin_load_view( view );
        }
    }

    emit_signal( "current_view_changed", m_active_path, view_id );

    return true;
}

void MFManager::begin_load_view( int view_index )
{
    const auto* view = m_level.fbs()->views()->Get( view_index );

    const int width  = static_cast<int>( view->res_x() );
    const int height = static_cast<int>( view->res_y() );

    auto pending = std::make_shared<PendingView>();

    // Allocate Images on the main thread; the worker writes into their raw buffers.
    pending->color_direct   = godot::Image::create( width, height, false, godot::Image::FORMAT_RGBF );
    pending->color_indirect = godot::Image::create( width, height, false, godot::Image::FORMAT_RGBF );
    pending->depth          = godot::Image::create( width, height, false, godot::Image::FORMAT_RF );
    pending->env            = godot::Image::create( width, height, false, godot::Image::FORMAT_RGBF );
    pending->light_dir      = godot::Image::create( width, height, false, godot::Image::FORMAT_RGBF );

    pending->buf_color_direct   = const_cast<uint8_t*>( pending->color_direct->get_data().ptr() );
    pending->buf_color_indirect = const_cast<uint8_t*>( pending->color_indirect->get_data().ptr() );
    pending->buf_depth          = const_cast<uint8_t*>( pending->depth->get_data().ptr() );
    pending->buf_env            = const_cast<uint8_t*>( pending->env->get_data().ptr() );
    pending->buf_light_dir      = const_cast<uint8_t*>( pending->light_dir->get_data().ptr() );

    // Pre-extract all file offsets and pixel sizes before dispatching — avoids any
    // cross-thread access to the flatbuffer or the Level struct.
    const size_t blob = m_level.blob_start_offset;
    const size_t px3  = static_cast<size_t>( width ) * height * 3 * sizeof( float );
    const size_t px1  = static_cast<size_t>( width ) * height * sizeof( float );

    struct ImageLoad
    {
        size_t file_offset;
        size_t compressed_size;
        void* out_buf;
        size_t pixel_bytes;
    };

    const std::vector<ImageLoad> loads = {
        { blob + static_cast<size_t>( view->color_direct()->offset() ), static_cast<size_t>( view->color_direct()->size() ), pending->buf_color_direct, px3 },
        { blob + static_cast<size_t>( view->color_indirect()->offset() ), static_cast<size_t>( view->color_indirect()->size() ), pending->buf_color_indirect,
          px3 },
        { blob + static_cast<size_t>( view->depth()->offset() ), static_cast<size_t>( view->depth()->size() ), pending->buf_depth, px1 },
        { blob + static_cast<size_t>( view->environment()->offset() ), static_cast<size_t>( view->environment()->size() ), pending->buf_env, px3 },
        { blob + static_cast<size_t>( view->light_direction()->offset() ), static_cast<size_t>( view->light_direction()->size() ), pending->buf_light_dir,
          px3 },
    };

    m_pending[view_index] = pending;

    std::string file_path = m_level.mflevel_path.string();

    std::thread(
        [pending, loads, file_path = std::move( file_path )]()
        {
            std::ifstream file( file_path, std::ios::binary );
            if( !file )
            {
                pending->ready.store( true, std::memory_order_release );
                return;
            }

            for( const auto& img : loads )
            {
                std::vector<char> compressed( img.compressed_size );
                file.seekg( static_cast<std::streamoff>( img.file_offset ) );
                file.read( compressed.data(), static_cast<std::streamsize>( img.compressed_size ) );
                if( !file )
                    break;
                mft::jxl::decode_jxl( compressed.data(), img.compressed_size, img.out_buf, img.pixel_bytes );
            }

            pending->ready.store( true, std::memory_order_release );
        } )
        .detach();
}

void MFManager::poll_pending()
{
    for( auto it = m_pending.begin(); it != m_pending.end(); )
    {
        if( !it->second->ready.load( std::memory_order_acquire ) )
        {
            ++it;
            continue;
        }

        const int view_id        = it->first;
        const auto* view         = m_level.fbs()->views()->Get( view_id );
        const auto& pending_view = *it->second;

        ViewCache cache;
        cache.color_direct   = pending_view.color_direct;
        cache.color_indirect = pending_view.color_indirect;
        cache.depth          = pending_view.depth;
        cache.env            = pending_view.env;
        cache.light_dir      = pending_view.light_dir;
        cache.transform      = view_transform( view->world_transform() );

        m_loaded[view_id] = std::move( cache );
        it                = m_pending.erase( it );

        emit_signal( "view_data_ready", view_id );
    }
}

bool MFManager::is_view_loaded( int view_id ) const
{
    return m_loaded.find( view_id ) != m_loaded.end();
}

const MFManager::ViewCache* MFManager::get_view_cache( int view_id ) const
{
    const auto found = m_loaded.find( view_id );
    return found != m_loaded.end() ? &found->second : nullptr;
}

void MFManager::_bind_methods()
{
    ADD_SIGNAL( godot::MethodInfo( "current_view_changed", godot::PropertyInfo( godot::Variant::STRING, "path" ),
                                   godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );
    ADD_SIGNAL( godot::MethodInfo( "view_data_ready", godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );

    godot::ClassDB::bind_method( godot::D_METHOD( "load", "path" ), &MFManager::load );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_active_path" ), &MFManager::get_active_path );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_num_views" ), &MFManager::get_num_views );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_closest_view_id", "point" ), &MFManager::get_closest_view_id );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_current_view", "view_id" ), &MFManager::set_current_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_navmesh" ), &MFManager::get_navmesh );
    godot::ClassDB::bind_method( godot::D_METHOD( "is_view_loaded", "view_id" ), &MFManager::is_view_loaded );
}
