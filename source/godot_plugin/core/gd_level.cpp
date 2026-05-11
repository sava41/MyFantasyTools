#include "gd_level.h"

#include "gd_manager.h"

#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

MFLevel::MFLevel( const godot::String& path )
    : m_background_material( memnew( godot::ShaderMaterial ) )
    , m_sky_material( memnew( godot::PanoramaSkyMaterial ) )
    , m_min_view_duration( 0.5 )
    , m_level_file_path( path )
{
    m_editor_mode = godot::Engine::get_singleton()->is_editor_hint();
}

MFLevel::~MFLevel()
{
}

void MFLevel::_enter_tree()
{
    MFManager::get()->connect( "current_view_changed", godot::Callable( this, "_on_view_changed" ) );

    m_min_view_duration_timer = memnew( godot::Timer() );
    m_min_view_duration_timer->set_one_shot( true );
    add_child( m_min_view_duration_timer );

    m_collision = memnew( godot::StaticBody3D() );
    m_collision->set_owner( this );
    add_child( m_collision );

    if( !m_editor_mode )
    {
        m_background = memnew( godot::MeshInstance3D() );
        m_background->set_gi_mode( godot::GeometryInstance3D::GI_MODE_DISABLED );
        m_background->set_cast_shadows_setting( godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF );

        m_game_camera = memnew( godot::Camera3D() );
        m_game_camera->set_keep_aspect_mode( godot::Camera3D::KEEP_WIDTH );
        m_game_camera->add_child( m_background );
        add_child( m_game_camera );

        godot::String shader = godot::FileAccess::get_file_as_string( "res://addons/mft_godot_plugin/background.gdshader" );

        godot::Ref<godot::Shader> background_shader = memnew( godot::Shader );
        background_shader->set_code( shader );

        m_background_material->set_render_priority( godot::Material::RENDER_PRIORITY_MAX );
        m_background_material->set_shader( background_shader );

        godot::Ref<godot::QuadMesh> background_mesh = memnew( godot::QuadMesh );
        background_mesh->set_size( { 2.0, 2.0 } );
        background_mesh->set_flip_faces( false );
        background_mesh->set_material( m_background_material );

        m_background->set_mesh( background_mesh );

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
    MFManager::get()->disconnect( "current_view_changed", godot::Callable( this, "_on_view_changed" ) );
}

void MFLevel::_ready()
{
}

void MFLevel::_process( double /*delta*/ )
{
    MFManager::get()->poll_pending();

    if( m_pending_view_id != -1 && MFManager::get()->is_view_loaded( m_pending_view_id ) )
    {
        apply_view( m_pending_view_id );
        m_pending_view_id = -1;
    }
}

void MFLevel::_on_view_changed( godot::String path, int view_id )
{
    if( path != m_level_file_path )
        return;
    set_view( view_id );
}

void MFLevel::update_shadows()
{
    if( m_editor_mode || !m_background_material.is_valid() )
    {
        return;
    }

    // Find all CollisionShape3D nodes with capsule shapes that have the shadow metadata
    const int MAX_CAPSULES = 10;
    godot::PackedVector3Array capsule_starts;
    godot::PackedVector3Array capsule_ends;
    godot::PackedFloat32Array capsule_radii;

    capsule_starts.resize( MAX_CAPSULES );
    capsule_ends.resize( MAX_CAPSULES );
    capsule_radii.resize( MAX_CAPSULES );

    int capsule_count = 0;

    godot::TypedArray<godot::Node> nodes = get_tree()->get_nodes_in_group( "capsule_shadow" );

    for( int i = 0; i < nodes.size() && capsule_count < MAX_CAPSULES; i++ )
    {
        godot::CollisionShape3D* collision_shape = godot::Object::cast_to<godot::CollisionShape3D>( nodes[i] );
        if( !collision_shape )
            continue;

        godot::Ref<godot::CapsuleShape3D> capsule_shape = collision_shape->get_shape();
        if( !capsule_shape.is_valid() )
            continue;

        godot::Transform3D world_transform = collision_shape->get_global_transform();
        godot::Vector3 world_pos           = world_transform.origin;

        float height = capsule_shape->get_height();
        float radius = capsule_shape->get_radius();

        godot::Vector3 scale = world_transform.basis.get_scale();

        float half_height        = ( height * 0.5f - radius ) * scale.y;
        float scaled_radius      = radius * ( ( scale.x + scale.z ) * 0.5f );
        godot::Vector3 local_dir = godot::Vector3( 0, 1, 0 );
        godot::Vector3 world_dir = world_transform.basis.xform( local_dir ).normalized();

        capsule_starts[capsule_count] = world_pos - world_dir * half_height;
        capsule_ends[capsule_count]   = world_pos + world_dir * half_height;
        capsule_radii[capsule_count]  = scaled_radius;

        capsule_count++;
    }

    m_background_material->set_shader_parameter( "capsule_starts", capsule_starts );
    m_background_material->set_shader_parameter( "capsule_ends", capsule_ends );
    m_background_material->set_shader_parameter( "capsule_radii", capsule_radii );
    m_background_material->set_shader_parameter( "capsule_count", capsule_count );
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
        return false;

    if( !MFManager::get()->load( m_level_file_path ) )
        return false;

    m_cur_view_id = view_id;

    if( MFManager::get()->is_view_loaded( view_id ) )
    {
        apply_view( view_id );
        m_pending_view_id = -1;
    }
    else
    {
        m_pending_view_id = view_id;
    }

    return true;
}

void MFLevel::apply_view( int view_id )
{
    const MFManager::ViewCache* cache = MFManager::get()->get_view_cache( view_id );
    if( !cache )
        return;

    const mft::Level& level = MFManager::get()->get_level();
    const auto* view        = level.fbs()->views()->Get( view_id );

    m_background_material->set_shader_parameter( "color_direct", godot::ImageTexture::create_from_image( cache->color_direct ) );
    m_background_material->set_shader_parameter( "color_indirect", godot::ImageTexture::create_from_image( cache->color_indirect ) );
    m_background_material->set_shader_parameter( "depth", godot::ImageTexture::create_from_image( cache->depth ) );
    m_background_material->set_shader_parameter( "light_direction", godot::ImageTexture::create_from_image( cache->light_dir ) );

    m_game_camera->set_current( true );
    m_cur_view_transform = setup_camera( level, view_id, m_game_camera );

    m_background_material->set_shader_parameter( "fov", view->cropped_fov() );
    m_background_material->set_shader_parameter( "uncropped_fov", view->fov() );
    m_background_material->set_shader_parameter( "uncropped_aspect", view->aspect() );
    m_background_material->set_shader_parameter( "uncropped_view_mat", m_cur_view_transform );

    m_sky_material->set_panorama( godot::ImageTexture::create_from_image( cache->env ) );

    godot::UtilityFunctions::print( "set camera: ", godot::String( view->name()->c_str() ) );

    m_min_view_duration_timer->start( m_min_view_duration );

    emit_signal( "view_changed", view_id );
}

// this function currently assumes the camera fov is set to cropped_fov
bool MFLevel::look_at( godot::Vector3 point, bool clamp_region, float smooth )
{
    if( m_editor_mode )
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
    if( m_min_view_duration_timer->get_time_left() != 0.0 )
        return false;

    if( !MFManager::get()->load( m_level_file_path ) )
        return false;

    const int view_id = MFManager::get()->get_closest_view_id( point );
    return MFManager::get()->set_current_view( view_id );
}

void MFLevel::set_min_view_duration( float timeMS )
{
    m_min_view_duration = timeMS;
}

float MFLevel::get_min_view_duration() const
{
    return m_min_view_duration;
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

    godot::ClassDB::bind_method( godot::D_METHOD( "_on_view_changed", "path", "view_id" ), &MFLevel::_on_view_changed );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_view", "viewId" ), &MFLevel::set_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_closest_view", "point" ), &MFLevel::set_closest_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "look_at", "point", "clamp_region", "smooth_rate" ), &MFLevel::look_at );
    godot::ClassDB::bind_method( godot::D_METHOD( "update_shadows" ), &MFLevel::update_shadows );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_min_view_duration", "time ms" ), &MFLevel::set_min_view_duration );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_min_view_duration" ), &MFLevel::get_min_view_duration );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_level_file_path", "relative path to level file" ), &MFLevel::set_level_file_path );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_level_file_path" ), &MFLevel::get_level_file_path );

    // Properties.
    ADD_GROUP( "View Properties", "" );
    ADD_PROPERTY( godot::PropertyInfo( godot::Variant::FLOAT, "min_view_duration", godot::PROPERTY_HINT_RANGE, "0.0,10.0,0.01,or_greater,suffix:s" ),
                  "set_min_view_duration", "get_min_view_duration" );
    ADD_PROPERTY( godot::PropertyInfo( godot::Variant::STRING, "level_file_path", godot::PROPERTY_HINT_FILE, "*.mflevel" ), "set_level_file_path",
                  "get_level_file_path" );
}
