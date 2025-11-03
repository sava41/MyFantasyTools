#include "gd_level.h"

#include "gd_manager.h"

#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

MFLevel::MFLevel( const godot::String& path )
    : m_backgroundMaterial( memnew( godot::ShaderMaterial ) )
    , m_skyMaterial( memnew( godot::PanoramaSkyMaterial ) )
    , m_minViewDuration( 0.5 )
    , m_levelFilePath( path )
{
    m_editorMode = godot::Engine::get_singleton()->is_editor_hint();
}

MFLevel::~MFLevel()
{
}

void MFLevel::_enter_tree()
{
    MFManager::get()->connect( "current_view_changed", godot::Callable( this, "set_view" ) );

    m_minViewDurationtimer = memnew( godot::Timer() );
    m_minViewDurationtimer->set_one_shot( true );
    add_child( m_minViewDurationtimer );

    m_collision = memnew( godot::StaticBody3D() );
    m_collision->set_owner( this );
    add_child( m_collision );

    if( !m_editorMode )
    {
        m_background = memnew( godot::MeshInstance3D() );
        m_background->set_gi_mode( godot::GeometryInstance3D::GI_MODE_DISABLED );
        m_background->set_cast_shadows_setting( godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF );

        m_gameCamera = memnew( godot::Camera3D() );
        m_gameCamera->set_keep_aspect_mode( godot::Camera3D::KEEP_WIDTH );
        m_gameCamera->add_child( m_background );
        add_child( m_gameCamera );

        godot::String shader = godot::FileAccess::get_file_as_string( "/Users/savanozin/Documents/MyFantasyGodot/demo/background.gdshader" );

        godot::Ref<godot::Shader> backgroundShader = memnew( godot::Shader );
        backgroundShader->set_code( shader );

        m_backgroundMaterial->set_render_priority( godot::Material::RENDER_PRIORITY_MAX );
        m_backgroundMaterial->set_shader( backgroundShader );

        godot::Ref<godot::QuadMesh> backgroundMesh = memnew( godot::QuadMesh );
        backgroundMesh->set_size( { 2.0, 2.0 } );
        backgroundMesh->set_flip_faces( true );
        backgroundMesh->set_material( m_backgroundMaterial );

        m_background->set_mesh( backgroundMesh );

        godot::Ref<godot::Sky> sky = memnew( godot::Sky );
        sky->set_material( m_skyMaterial );
        sky->set_process_mode( godot::Sky::PROCESS_MODE_REALTIME );
        sky->set_radiance_size( godot::Sky::RADIANCE_SIZE_256 );

        godot::Ref<godot::Environment> environment = memnew( godot::Environment );
        environment->set_sky( sky );

        // mft panoramas are z+ forward while godod uses z- forward
        environment->set_sky_rotation( godot::Vector3( 0.0, Math_PI, 0.0 ) );
        environment->set_ambient_source( godot::Environment::AMBIENT_SOURCE_SKY );
        environment->set_reflection_source( godot::Environment::REFLECTION_SOURCE_SKY );
        environment->set_tonemapper( godot::Environment::TONE_MAPPER_AGX );
        environment->set_tonemap_exposure( 8.0 );
        m_gameCamera->set_environment( environment );

        godot::UtilityFunctions::print( "created game camera" );
    }

    m_levelReady = true;

    initialize_level_data();
}

void MFLevel::_ready()
{
}

void MFLevel::initialize_level_data()
{

    if( m_levelFilePath.is_empty() || !m_levelReady )
    {
        return;
    }

    if( !MFManager::get()->load( m_levelFilePath ) )
    {
        godot::UtilityFunctions::print( "load level failed" );
        return;
    }

    setup_navmesh();

    setup_cameras();
}

void MFLevel::setup_cameras()
{
    if( m_editorMode )
    {
        for( godot::Camera3D* camera : m_editorCameras )
        {
            camera->queue_free();
        }
        m_editorCameras.clear();

        for( int i = 0; i < MFManager::get()->get_num_views(); ++i )
        {
            const std::unique_ptr<GDViewResources>& view_data = MFManager::get()->get_view_data( i );

            godot::Camera3D* camera = memnew( godot::Camera3D() );
            camera->set_name( godot::String( view_data->m_view_info->name()->c_str() ) );
            camera->set_transform( view_data->m_transform );

            add_child( camera );
            camera->set_owner( this );

            m_editorCameras.push_back( camera );

            godot::UtilityFunctions::print( "made camera ", view_data->m_view_info->name()->c_str() );
        }
    }
}

void MFLevel::setup_navmesh()
{
    godot::Ref<godot::ConcavePolygonShape3D> navmesh = memnew( godot::ConcavePolygonShape3D );
    navmesh->set_faces( MFManager::get()->get_navmesh() );

    godot::CollisionShape3D* collisionShape = memnew( godot::CollisionShape3D() );
    collisionShape->set_shape( navmesh );

    m_collision->add_child( collisionShape );

    collisionShape->set_owner( this );
}

bool MFLevel::set_view( int view_id )
{
    if( m_editorMode )
    {
        return false;
    }

    if( MFManager::get()->get_current_level() != m_levelFilePath )
    {
        m_gameCamera->set_current( false );
        return false;
    }

    int m_cur_camera_index = view_id;

    const std::unique_ptr<GDViewResources>& view_data = MFManager::get()->get_view_data( view_id );

    m_backgroundMaterial->set_shader_parameter( "color", godot::ImageTexture::create_from_image( view_data->m_colorBuffer ) );
    m_backgroundMaterial->set_shader_parameter( "depth", godot::ImageTexture::create_from_image( view_data->m_depthBuffer ) );

    m_skyMaterial->set_panorama( godot::ImageTexture::create_from_image( view_data->m_envBuffer ) );

    m_gameCamera->set_current( true );
    m_gameCamera->set_transform( view_data->m_transform );
    m_gameCamera->set_fov( view_data->m_view_info->fov() );

    godot::UtilityFunctions::print( "set camera: ", godot::String( view_data->m_view_info->name()->c_str() ) );

    m_minViewDurationtimer->start( m_minViewDuration );

    return true;
}

bool MFLevel::look_at( const godot::Vector3& point )
{
    const std::unique_ptr<GDViewResources>& view_data = MFManager::get()->get_view_data( m_cur_camera_index );
}

bool MFLevel::set_closest_view( const godot::Vector3& point )
{
    if( m_minViewDurationtimer->get_time_left() != 0.0 )
    {
        return false;
    }

    const int viewId = MFManager::get()->get_closest_view_Id( point );
    return MFManager::get()->set_current_view( viewId );
}

void MFLevel::set_min_view_duration( float timeMS )
{
    m_minViewDuration = timeMS;
}

float MFLevel::get_min_view_duration() const
{
    return m_minViewDuration;
}

void MFLevel::set_level_file_path( const godot::String& path )
{
    if( path != m_levelFilePath )
    {
        m_levelFilePath = path;
        initialize_level_data();
    }
}

godot::String MFLevel::get_level_file_path() const
{
    return m_levelFilePath;
}

void MFLevel::_bind_methods()
{
    godot::ClassDB::bind_method( godot::D_METHOD( "set_view", "viewId" ), &MFLevel::set_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_closest_view", "point" ), &MFLevel::set_closest_view );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_min_view_duration", "time ms" ), &MFLevel::set_min_view_duration );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_min_view_duration" ), &MFLevel::get_min_view_duration );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_level_file_path", "relative path to level file" ), &MFLevel::set_level_file_path );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_level_file_path" ), &MFLevel::get_level_file_path );

    // Properties.
    ADD_GROUP( "View Properties", "group_" );
    ADD_PROPERTY( godot::PropertyInfo( godot::Variant::FLOAT, "group_min_view_duration" ), "set_min_view_duration", "get_min_view_duration" );
    ADD_PROPERTY( godot::PropertyInfo( godot::Variant::STRING, "group_level_file_path" ), "set_level_file_path", "get_level_file_path" );
}