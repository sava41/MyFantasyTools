#include "gd_level.h"

#include "gd_manager.h"

#include <godot_cpp/classes/compositor.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

MFLevel::MFLevel( const godot::String& path )
    : m_sky_material( memnew( godot::PanoramaSkyMaterial ) )
    , m_level_file_path( path )
{
    m_editor_mode = godot::Engine::get_singleton()->is_editor_hint();
}

MFLevel::~MFLevel()
{
}

void MFLevel::_enter_tree()
{
    MFManager::get()->connect( "view_data_ready", godot::Callable( this, "_on_view_data_ready" ) );
    MFManager::get()->connect( "level_unloaded", godot::Callable( this, "_on_level_unloaded" ) );

    m_collision = memnew( godot::StaticBody3D() );
    m_collision->set_owner( this );
    add_child( m_collision );

    if( !m_editor_mode )
    {
        m_background_effect = memnew( MFBackgroundEffect() );

        godot::Array effects;
        effects.push_back( m_background_effect );
        godot::Ref<godot::Compositor> compositor = memnew( godot::Compositor );
        compositor->set_compositor_effects( effects );

        m_game_camera = memnew( godot::Camera3D() );
        m_game_camera->set_keep_aspect_mode( godot::Camera3D::KEEP_WIDTH );
        m_game_camera->set_compositor( compositor );
        add_child( m_game_camera );

        godot::Ref<godot::Sky> sky = memnew( godot::Sky );
        sky->set_material( m_sky_material );
        sky->set_process_mode( godot::Sky::PROCESS_MODE_REALTIME );
        sky->set_radiance_size( godot::Sky::RADIANCE_SIZE_256 );

        godot::Ref<godot::Environment> environment = memnew( godot::Environment );
        environment->set_sky( sky );

        // mft panoramas are z+ forward while godot uses z- forward
        environment->set_sky_rotation( godot::Vector3( 0.0, Math_PI, 0.0 ) );
        environment->set_ambient_source( godot::Environment::AMBIENT_SOURCE_SKY );
        environment->set_reflection_source( godot::Environment::REFLECTION_SOURCE_SKY );
        environment->set_tonemapper( godot::Environment::TONE_MAPPER_AGX );
        environment->set_tonemap_exposure( 8.0 );
        m_game_camera->set_environment( environment );

        godot::UtilityFunctions::print( "created game camera" );

        set_process( true );
    }

    m_level_ready = true;

    initialize_level_data();
}

void MFLevel::_exit_tree()
{
    MFManager::get()->disconnect( "view_data_ready", godot::Callable( this, "_on_view_data_ready" ) );
    MFManager::get()->disconnect( "level_unloaded", godot::Callable( this, "_on_level_unloaded" ) );
}

void MFLevel::_ready()
{
}

void MFLevel::_process( double /*delta*/ )
{
}

void MFLevel::_on_level_unloaded( godot::String path )
{
    if( path != m_level_file_path )
        return;
    m_cur_view_id     = -1;
    m_pending_view_id = -1;
}

void MFLevel::_on_view_data_ready( godot::String path, int view_id )
{
    if( path != m_level_file_path )
    {
        return;
    }
    if( view_id != m_pending_view_id )
    {
        return;
    }

    apply_view( view_id );
}

void MFLevel::initialize_level_data()
{
    if( m_level_file_path.is_empty() || !m_level_ready )
        return;

    if( !MFManager::get()->load( m_level_file_path ) )
    {
        godot::UtilityFunctions::print( "load level failed" );
        emit_signal( "level_load_failed", m_level_file_path );
        return;
    }

    setup_navmesh();
    setup_editor_cameras();

    emit_signal( "level_loaded", m_level_file_path );
}

godot::Transform3D MFLevel::setup_camera( const mft::Level& level, int view_index, godot::Camera3D* camera )
{
    const auto* view = level.fbs()->views()->Get( view_index );
    const auto* mat  = view->world_transform();

    // Build y-up Transform3D from z-up flatbuffer Mat4.
    godot::Transform3D transform( mat->m00(), mat->m01(), mat->m02(), mat->m10(), mat->m11(), mat->m12(), mat->m20(), mat->m21(), mat->m22(), mat->m03(),
                                  mat->m13(), mat->m23() );
    transform.rotate( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 );

    camera->set_transform( transform );
    camera->set_fov( godot::Math::rad_to_deg( view->cropped_fov() ) );

    return transform;
}

void MFLevel::setup_editor_cameras()
{
    if( !m_editor_mode )
        return;

    for( godot::Camera3D* camera : m_editor_cameras )
        camera->queue_free();
    m_editor_cameras.clear();

    const mft::Level& level = MFManager::get()->get_level();
    if( !level.fbs() || !level.fbs()->views() )
        return;

    for( int i = 0; i < static_cast<int>( level.fbs()->views()->size() ); ++i )
    {
        const auto* view = level.fbs()->views()->Get( i );

        godot::Camera3D* camera = memnew( godot::Camera3D() );
        camera->set_name( godot::String( view->name()->c_str() ) );
        setup_camera( level, i, camera );

        add_child( camera );
        camera->set_owner( this );

        m_editor_cameras.push_back( camera );

        godot::UtilityFunctions::print( "made camera ", view->name()->c_str() );
    }
}

void MFLevel::setup_navmesh()
{
    godot::Ref<godot::ConcavePolygonShape3D> navmesh = memnew( godot::ConcavePolygonShape3D );
    navmesh->set_faces( MFManager::get()->get_navmesh() );

    godot::CollisionShape3D* collision_shape = memnew( godot::CollisionShape3D() );
    collision_shape->set_shape( navmesh );

    m_collision->add_child( collision_shape );
    collision_shape->set_owner( this );
}

bool MFLevel::set_view( int view_id )
{
    if( m_editor_mode )
    {
        return false;
    }

    if( !MFManager::get()->load( m_level_file_path ) )
    {
        return false;
    }

    if( !MFManager::get()->load_from( view_id ) )
    {
        return false;
    }

    if( MFManager::get()->is_view_loaded( view_id ) )
    {
        apply_view( view_id );
    }
    else
    {
        m_pending_view_id = view_id;
    }

    return true;
}

bool MFLevel::apply_view( int view_id )
{
    if( view_id == m_cur_view_id )
    {
        return false;
    }

    m_cur_view_id = view_id;

    const MFManager::ViewCache* cache = MFManager::get()->get_view_cache( view_id );
    if( !cache )
    {
        return false;
    }

    const mft::Level& level = MFManager::get()->get_level();
    const auto* view        = level.fbs()->views()->Get( view_id );

    m_background_effect->set_view_textures(
        godot::ImageTexture::create_from_image( cache->color_direct ),
        godot::ImageTexture::create_from_image( cache->color_indirect ),
        godot::ImageTexture::create_from_image( cache->depth ) );

    m_game_camera->set_current( true );
    m_cur_view_transform = setup_camera( level, view_id, m_game_camera );

    m_background_effect->set_view_params(
        view->fov(),
        view->aspect(),
        m_cur_view_transform );

    m_sky_material->set_panorama( godot::ImageTexture::create_from_image( cache->env ) );

    godot::UtilityFunctions::print( "set camera: ", godot::String( view->name()->c_str() ) );

    return true;
}

// this function currently assumes the camera fov is set to cropped_fov
bool MFLevel::look_at( godot::Vector3 point, bool clamp_region, float smooth )
{
    if( m_editor_mode )
        return false;

    if( m_cur_view_id < 0 )
        return false;

    if( !MFManager::get()->load( m_level_file_path ) )
        return false;

    const auto* view = MFManager::get()->get_level().fbs()->views()->Get( m_cur_view_id );

    const float max_pan  = view->max_pan();
    const float max_tilt = view->max_tilt();

    if( max_pan == 0.0 && max_tilt == 0.0 )
        return false;

    const godot::Transform3D camera_transform_uncropped = m_cur_view_transform;
    const godot::Transform3D camera_transform           = m_game_camera->get_camera_transform();

    if( clamp_region )
    {
        const godot::Vector3 camera_space_point = camera_transform_uncropped.xform_inv( point );

        const float distance        = camera_space_point.length();
        const float horizontal_dist = sqrt( camera_space_point.x * camera_space_point.x + camera_space_point.z * camera_space_point.z );

        float pan  = atan2( camera_space_point.x, -camera_space_point.z );
        float tilt = atan2( camera_space_point.y, horizontal_dist );

        pan  = godot::Math::clamp( pan, -max_pan * 0.5f, max_pan * 0.5f );
        tilt = godot::Math::clamp( tilt, -max_tilt * 0.5f, max_tilt * 0.5f );

        const godot::Vector3 clamped_direction( cos( tilt ) * sin( pan ), sin( tilt ), -cos( tilt ) * cos( pan ) );

        point = camera_transform_uncropped.xform( clamped_direction * distance );
    }

    const godot::Vector3 camera_forward = -camera_transform.basis.rows[2];

    point = camera_forward.slerp( point, 1.0 - smooth );

    m_game_camera->set_transform( camera_transform.looking_at( point ) );

    return true;
}

bool MFLevel::set_closest_view( const godot::Vector3& point )
{
    const int view_id = MFManager::get()->get_closest_view_id( point );
    return set_view( view_id );
}

void MFLevel::set_level_file_path( const godot::String& path )
{
    if( path != m_level_file_path )
    {
        m_level_file_path = path;
        initialize_level_data();
    }
}

godot::String MFLevel::get_level_file_path() const
{
    return m_level_file_path;
}

void MFLevel::_bind_methods()
{
    ADD_SIGNAL( godot::MethodInfo( "level_loaded", godot::PropertyInfo( godot::Variant::STRING, "path" ) ) );
    ADD_SIGNAL( godot::MethodInfo( "level_load_failed", godot::PropertyInfo( godot::Variant::STRING, "path" ) ) );
    ADD_SIGNAL( godot::MethodInfo( "view_changed", godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );

    godot::ClassDB::bind_method( godot::D_METHOD( "_on_level_unloaded", "path" ), &MFLevel::_on_level_unloaded );
    godot::ClassDB::bind_method( godot::D_METHOD( "_on_view_data_ready", "path", "view_id" ), &MFLevel::_on_view_data_ready );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_view", "viewId" ), &MFLevel::set_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_closest_view", "point" ), &MFLevel::set_closest_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "look_at", "point", "clamp_region", "smooth_rate" ), &MFLevel::look_at );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_level_file_path", "relative path to level file" ), &MFLevel::set_level_file_path );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_level_file_path" ), &MFLevel::get_level_file_path );

    // Properties.
    ADD_PROPERTY( godot::PropertyInfo( godot::Variant::STRING, "level_file_path", godot::PROPERTY_HINT_FILE, "*.mflevel" ), "set_level_file_path",
                  "get_level_file_path" );
}
