#include "gd_background_effect.h"

#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MFBackgroundEffect::pack_projection( const godot::Projection& p, float* out )
{
    for( int c = 0; c < 4; c++ )
    {
        out[c * 4 + 0] = p.columns[c].x;
        out[c * 4 + 1] = p.columns[c].y;
        out[c * 4 + 2] = p.columns[c].z;
        out[c * 4 + 3] = p.columns[c].w;
    }
}

// Godot Basis stores rows, so rows[r][c] = element at (row r, col c).
// GLSL mat4 is column-major: out[col*4 + row].
void MFBackgroundEffect::pack_transform( const godot::Transform3D& t, float* out )
{
    const godot::Basis& b = t.basis;
    out[0]  = b.rows[0][0]; out[1]  = b.rows[1][0]; out[2]  = b.rows[2][0]; out[3]  = 0.0f;
    out[4]  = b.rows[0][1]; out[5]  = b.rows[1][1]; out[6]  = b.rows[2][1]; out[7]  = 0.0f;
    out[8]  = b.rows[0][2]; out[9]  = b.rows[1][2]; out[10] = b.rows[2][2]; out[11] = 0.0f;
    out[12] = t.origin.x;   out[13] = t.origin.y;   out[14] = t.origin.z;   out[15] = 1.0f;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MFBackgroundEffect::MFBackgroundEffect()
{
    set_effect_callback_type( EFFECT_CALLBACK_TYPE_POST_TRANSPARENT );
    set_enabled( true );
}

MFBackgroundEffect::~MFBackgroundEffect()
{
    godot::RenderingDevice* rd = godot::RenderingServer::get_singleton()->get_rendering_device();
    if( !rd ) return;
    if( m_params_buffer.is_valid() ) rd->free_rid( m_params_buffer );
    if( m_sampler.is_valid() )       rd->free_rid( m_sampler );
    if( m_pipeline.is_valid() )      rd->free_rid( m_pipeline );
    if( m_shader.is_valid() )        rd->free_rid( m_shader );
}

// ---------------------------------------------------------------------------
// Public setters (called by MFLevel::apply_view)
// ---------------------------------------------------------------------------

void MFBackgroundEffect::set_view_textures( godot::Ref<godot::ImageTexture> color_direct,
                                             godot::Ref<godot::ImageTexture> color_indirect,
                                             godot::Ref<godot::ImageTexture> depth_baked )
{
    m_color_direct   = color_direct;
    m_color_indirect = color_indirect;
    m_depth_baked    = depth_baked;
}

void MFBackgroundEffect::set_view_params( float uncropped_fov,
                                           float uncropped_aspect,
                                           godot::Transform3D uncropped_view_mat )
{
    m_uncropped_fov      = uncropped_fov;
    m_uncropped_aspect   = uncropped_aspect;
    m_uncropped_view_mat = uncropped_view_mat;
}

// ---------------------------------------------------------------------------
// RD pipeline initialisation (called once, lazily on first callback)
// ---------------------------------------------------------------------------

void MFBackgroundEffect::initialize( godot::RenderingDevice* rd )
{
    m_initialized = true;

    godot::Ref<godot::RDShaderFile> shader_file = godot::ResourceLoader::get_singleton()->load(
        "res://addons/mft_godot_plugin/background_composite.glsl" );

    if( !shader_file.is_valid() )
    {
        godot::UtilityFunctions::printerr( "MFBackgroundEffect: could not load background_composite.glsl" );
        return;
    }

    godot::Ref<godot::RDShaderSPIRV> spirv = shader_file->get_spirv();
    if( !spirv.is_valid() )
    {
        godot::UtilityFunctions::printerr( "MFBackgroundEffect: shader has no SPIRV" );
        return;
    }

    m_shader   = rd->shader_create_from_spirv( spirv );
    m_pipeline = rd->compute_pipeline_create( m_shader );

    // Linear-clamp sampler for baked textures
    godot::Ref<godot::RDSamplerState> ss;
    ss.instantiate();
    ss->set_min_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_mag_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_mip_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_repeat_u( godot::RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE );
    ss->set_repeat_v( godot::RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE );
    m_sampler = rd->sampler_create( ss );

    // Persistent params UBO
    m_params_buffer = rd->uniform_buffer_create( sizeof( BackgroundParams ) );
}

// ---------------------------------------------------------------------------
// Per-frame render callback
// ---------------------------------------------------------------------------

void MFBackgroundEffect::_render_callback( int /*type*/, godot::RenderData* render_data )
{
    if( !m_color_direct.is_valid() || !m_color_indirect.is_valid() || !m_depth_baked.is_valid() )
        return;

    godot::RenderingDevice* rd = godot::RenderingServer::get_singleton()->get_rendering_device();
    if( !rd || !render_data ) return;

    if( !m_initialized ) initialize( rd );
    if( !m_pipeline.is_valid() ) return;

    // Scene buffers & render size
    godot::Ref<godot::RenderSceneBuffersRD> buffers = render_data->get_render_scene_buffers();
    if( !buffers.is_valid() ) return;

    godot::Vector2i size = buffers->get_internal_size();
    if( size.x == 0 || size.y == 0 ) return;

    godot::RID color_tex = buffers->get_color_texture();
    godot::RID depth_tex = buffers->get_depth_texture();

    // Camera matrices
    godot::RenderSceneData* scene_data = render_data->get_render_scene_data();
    godot::Transform3D cam_transform   = scene_data->get_cam_transform();
    godot::Projection  cam_proj        = scene_data->get_cam_projection();

    // Build and upload params
    BackgroundParams p;
    pack_projection( cam_proj.inverse(), p.inv_projection );
    pack_projection( cam_proj,            p.projection );
    pack_transform( cam_transform.affine_inverse(), p.view_matrix );
    pack_transform( cam_transform,                  p.inv_view_matrix );
    pack_transform( m_uncropped_view_mat,            p.uncropped_view_mat );
    pack_transform( m_uncropped_view_mat.affine_inverse(), p.inv_uncropped_view_mat );
    p.uncropped_fov    = m_uncropped_fov;
    p.uncropped_aspect = m_uncropped_aspect;
    p.screen_w         = size.x;
    p.screen_h         = size.y;

    godot::PackedByteArray params_bytes;
    params_bytes.resize( sizeof( BackgroundParams ) );
    memcpy( params_bytes.ptrw(), &p, sizeof( BackgroundParams ) );
    rd->buffer_update( m_params_buffer, 0, params_bytes.size(), params_bytes );

    // Baked textures: RS RID → RD RID
    godot::RenderingServer* rs   = godot::RenderingServer::get_singleton();
    godot::RID color_direct_rd   = rs->texture_get_rd_texture( m_color_direct->get_rid() );
    godot::RID color_indirect_rd = rs->texture_get_rd_texture( m_color_indirect->get_rid() );
    godot::RID depth_baked_rd    = rs->texture_get_rd_texture( m_depth_baked->get_rid() );

    // Set 0: color image output
    godot::Ref<godot::RDUniform> u0;
    u0.instantiate();
    u0->set_uniform_type( godot::RenderingDevice::UNIFORM_TYPE_IMAGE );
    u0->set_binding( 0 );
    u0->add_id( color_tex );
    godot::TypedArray<godot::RDUniform> set0;
    set0.push_back( u0 );
    godot::RID us0 = rd->uniform_set_create( set0, m_shader, 0 );

    // Set 1: sampled textures
    auto make_sampled = [&]( int binding, godot::RID tex ) -> godot::Ref<godot::RDUniform>
    {
        godot::Ref<godot::RDUniform> u;
        u.instantiate();
        u->set_uniform_type( godot::RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE );
        u->set_binding( binding );
        u->add_id( m_sampler );
        u->add_id( tex );
        return u;
    };
    godot::TypedArray<godot::RDUniform> set1;
    set1.push_back( make_sampled( 0, color_direct_rd ) );
    set1.push_back( make_sampled( 1, color_indirect_rd ) );
    set1.push_back( make_sampled( 2, depth_baked_rd ) );
    set1.push_back( make_sampled( 3, depth_tex ) );
    godot::RID us1 = rd->uniform_set_create( set1, m_shader, 1 );

    // Set 2: params UBO
    godot::Ref<godot::RDUniform> u2;
    u2.instantiate();
    u2->set_uniform_type( godot::RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER );
    u2->set_binding( 0 );
    u2->add_id( m_params_buffer );
    godot::TypedArray<godot::RDUniform> set2;
    set2.push_back( u2 );
    godot::RID us2 = rd->uniform_set_create( set2, m_shader, 2 );

    // Dispatch compute
    int64_t cl = rd->compute_list_begin();
    rd->compute_list_bind_compute_pipeline( cl, m_pipeline );
    rd->compute_list_bind_uniform_set( cl, us0, 0 );
    rd->compute_list_bind_uniform_set( cl, us1, 1 );
    rd->compute_list_bind_uniform_set( cl, us2, 2 );
    rd->compute_list_dispatch( cl, ( size.x + 7 ) / 8, ( size.y + 7 ) / 8, 1 );
    rd->compute_list_end();

    rd->free_rid( us0 );
    rd->free_rid( us1 );
    rd->free_rid( us2 );
}

void MFBackgroundEffect::_bind_methods()
{
}
