#include "gd_background.h"

#include <godot_cpp/classes/rd_pipeline_color_blend_state.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state_attachment.hpp>
#include <godot_cpp/classes/rd_pipeline_depth_stencil_state.hpp>
#include <godot_cpp/classes/rd_pipeline_multisample_state.hpp>
#include <godot_cpp/classes/rd_pipeline_rasterization_state.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rd_vertex_attribute.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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
    out[0]                = b.rows[0][0];
    out[1]                = b.rows[1][0];
    out[2]                = b.rows[2][0];
    out[3]                = 0.0f;
    out[4]                = b.rows[0][1];
    out[5]                = b.rows[1][1];
    out[6]                = b.rows[2][1];
    out[7]                = 0.0f;
    out[8]                = b.rows[0][2];
    out[9]                = b.rows[1][2];
    out[10]               = b.rows[2][2];
    out[11]               = 0.0f;
    out[12]               = t.origin.x;
    out[13]               = t.origin.y;
    out[14]               = t.origin.z;
    out[15]               = 1.0f;
}

MFBackgroundEffect::MFBackgroundEffect()
{
    set_effect_callback_type( EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT );
    set_enabled( true );
}

MFBackgroundEffect::~MFBackgroundEffect()
{
    godot::RenderingDevice* rd = godot::RenderingServer::get_singleton()->get_rendering_device();
    if( !rd )
        return;
    if( m_params_buffer.is_valid() )
        rd->free_rid( m_params_buffer );
    if( m_sampler.is_valid() )
        rd->free_rid( m_sampler );
    if( m_params_uniform_set.is_valid() )
        rd->free_rid( m_params_uniform_set );
    if( m_vertex_array.is_valid() )
        rd->free_rid( m_vertex_array );
    if( m_vertex_buffer.is_valid() )
        rd->free_rid( m_vertex_buffer );
    if( m_pipeline.is_valid() )
        rd->free_rid( m_pipeline );
    if( m_shader.is_valid() )
        rd->free_rid( m_shader );
}

void MFBackgroundEffect::set_view_textures( godot::Ref<godot::ImageTexture> color_direct, godot::Ref<godot::ImageTexture> color_indirect,
                                            godot::Ref<godot::ImageTexture> depth_baked )
{
    m_color_direct   = color_direct;
    m_color_indirect = color_indirect;
    m_depth_baked    = depth_baked;
}

void MFBackgroundEffect::set_view_params( float uncropped_fov, float uncropped_aspect, godot::Transform3D uncropped_view_mat )
{
    m_uncropped_fov      = uncropped_fov;
    m_uncropped_aspect   = uncropped_aspect;
    m_uncropped_view_mat = uncropped_view_mat;
}

void MFBackgroundEffect::init( godot::RenderingDevice* rd, godot::RID color_tex )
{
    m_initialized = true;

    godot::Ref<godot::RDShaderFile> shader_file = godot::ResourceLoader::get_singleton()->load( "res://addons/mft_godot_plugin/background_composite.glsl" );

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

    m_shader = rd->shader_create_from_spirv( spirv );

    // Linear-clamp sampler for baked textures
    godot::Ref<godot::RDSamplerState> ss;
    ss.instantiate();
    ss->set_min_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_mag_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_mip_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_repeat_u( godot::RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE );
    ss->set_repeat_v( godot::RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE );
    m_sampler = rd->sampler_create( ss );

    // Persistent params UBO and its uniform set (set 0, shared by vertex and fragment)
    m_params_buffer = rd->uniform_buffer_create( sizeof( BackgroundParams ) );

    godot::Ref<godot::RDUniform> u_params;
    u_params.instantiate();
    u_params->set_uniform_type( godot::RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER );
    u_params->set_binding( 0 );
    u_params->add_id( m_params_buffer );
    godot::TypedArray<godot::RDUniform> params_set;
    params_set.push_back( u_params );
    m_params_uniform_set = rd->uniform_set_create( params_set, m_shader, 0 );

    // Fullscreen quad in uncropped NDC space (two triangles)
    float quad[] = {
        -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,
    };
    godot::PackedByteArray vdata;
    vdata.resize( sizeof( quad ) );
    memcpy( vdata.ptrw(), quad, sizeof( quad ) );
    m_vertex_buffer = rd->vertex_buffer_create( sizeof( quad ), vdata );

    godot::Ref<godot::RDVertexAttribute> attr;
    attr.instantiate();
    attr->set_location( 0 );
    attr->set_format( godot::RenderingDevice::DATA_FORMAT_R32G32_SFLOAT );
    attr->set_stride( 2 * sizeof( float ) );
    attr->set_offset( 0 );
    godot::TypedArray<godot::RDVertexAttribute> vertex_attrs;
    vertex_attrs.push_back( attr );
    int64_t vertex_format = rd->vertex_format_create( vertex_attrs );

    godot::TypedArray<godot::RID> vertex_bufs;
    vertex_bufs.push_back( m_vertex_buffer );
    godot::PackedInt64Array vertex_offsets;
    vertex_offsets.push_back( 0 );
    m_vertex_array = rd->vertex_array_create( 6, vertex_format, vertex_bufs, vertex_offsets );

    // Probe the framebuffer format from the actual color texture so it always matches.
    {
        godot::TypedArray<godot::RID> probe_atts;
        probe_atts.push_back( color_tex );
        godot::RID probe_fb  = rd->framebuffer_create( probe_atts );
        m_framebuffer_format = rd->framebuffer_get_format( probe_fb );
        rd->free_rid( probe_fb );
    }

    // Render pipeline
    godot::Ref<godot::RDPipelineRasterizationState> rast;
    rast.instantiate();

    godot::Ref<godot::RDPipelineMultisampleState> ms;
    ms.instantiate();

    godot::Ref<godot::RDPipelineDepthStencilState> ds;
    ds.instantiate();

    godot::Ref<godot::RDPipelineColorBlendStateAttachment> blend_att;
    blend_att.instantiate();
    godot::TypedArray<godot::RDPipelineColorBlendStateAttachment> blend_atts;
    blend_atts.push_back( blend_att );

    godot::Ref<godot::RDPipelineColorBlendState> blend;
    blend.instantiate();
    blend->set_attachments( blend_atts );

    m_pipeline =
        rd->render_pipeline_create( m_shader, m_framebuffer_format, vertex_format, godot::RenderingDevice::RENDER_PRIMITIVE_TRIANGLES, rast, ms, ds, blend );
}

static inline godot::Ref<godot::RDUniform> make_sampled( int binding, godot::RID tex, godot::RID sampler )
{
    godot::Ref<godot::RDUniform> u;
    u.instantiate();
    u->set_uniform_type( godot::RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE );
    u->set_binding( binding );
    u->add_id( sampler );
    u->add_id( tex );
    return u;
}

void MFBackgroundEffect::_render_callback( int /*type*/, godot::RenderData* render_data )
{
    if( !m_color_direct.is_valid() || !m_color_indirect.is_valid() || !m_depth_baked.is_valid() )
        return;

    godot::RenderingDevice* rd = godot::RenderingServer::get_singleton()->get_rendering_device();
    if( !rd || !render_data )
        return;

    // Scene buffers & render size
    godot::Ref<godot::RenderSceneBuffersRD> buffers = render_data->get_render_scene_buffers();
    if( !buffers.is_valid() )
        return;

    godot::Vector2i size = buffers->get_internal_size();
    if( size.x == 0 || size.y == 0 )
        return;

    godot::RID color_tex = buffers->get_color_texture();

    if( !m_initialized )
        init( rd, color_tex );
    if( !m_pipeline.is_valid() )
        return;
    godot::RID depth_tex = buffers->get_depth_texture();

    godot::RenderSceneData* scene_data = render_data->get_render_scene_data();
    godot::Transform3D cam_transform   = scene_data->get_cam_transform();
    godot::Projection cam_proj         = scene_data->get_cam_projection();

    godot::PackedByteArray params_bytes;
    params_bytes.resize( sizeof( BackgroundParams ) );
    BackgroundParams& p = *reinterpret_cast<BackgroundParams*>( params_bytes.ptrw() );
    pack_projection( cam_proj.inverse(), p.inv_projection );
    pack_projection( cam_proj, p.projection );
    pack_transform( cam_transform.affine_inverse(), p.view_matrix );
    pack_transform( cam_transform, p.inv_view_matrix );
    pack_transform( m_uncropped_view_mat, p.uncropped_view_mat );
    pack_transform( m_uncropped_view_mat.affine_inverse(), p.inv_uncropped_view_mat );
    p.uncropped_fov    = m_uncropped_fov;
    p.uncropped_aspect = m_uncropped_aspect;
    p.screen_w         = size.x;
    p.screen_h         = size.y;
    rd->buffer_update( m_params_buffer, 0, params_bytes.size(), params_bytes );

    godot::RenderingServer* rs   = godot::RenderingServer::get_singleton();
    godot::RID color_direct_rd   = rs->texture_get_rd_texture( m_color_direct->get_rid() );
    godot::RID color_indirect_rd = rs->texture_get_rd_texture( m_color_indirect->get_rid() );
    godot::RID depth_baked_rd    = rs->texture_get_rd_texture( m_depth_baked->get_rid() );

    // Set 1: baked textures + scene depth
    godot::TypedArray<godot::RDUniform> textures_set;
    textures_set.push_back( make_sampled( 0, color_direct_rd, m_sampler ) );
    textures_set.push_back( make_sampled( 1, color_indirect_rd, m_sampler ) );
    textures_set.push_back( make_sampled( 2, depth_baked_rd, m_sampler ) );
    textures_set.push_back( make_sampled( 3, depth_tex, m_sampler ) );
    godot::RID textures_uniform_set = rd->uniform_set_create( textures_set, m_shader, 1 );

    // Create a color-only framebuffer for this frame's color texture
    godot::TypedArray<godot::RID> fb_atts;
    fb_atts.push_back( color_tex );
    godot::RID framebuffer = rd->framebuffer_create( fb_atts, m_framebuffer_format );

    // Draw the quad — default DrawFlags (0) = load existing color, store result.
    // Pixels discarded by the fragment shader retain their 3D scene color.
    int64_t dl = rd->draw_list_begin( framebuffer );
    rd->draw_list_bind_render_pipeline( dl, m_pipeline );
    rd->draw_list_bind_uniform_set( dl, m_params_uniform_set, 0 );
    rd->draw_list_bind_uniform_set( dl, textures_uniform_set, 1 );
    rd->draw_list_bind_vertex_array( dl, m_vertex_array );
    rd->draw_list_draw( dl, false, 1 );
    rd->draw_list_end();

    rd->free_rid( framebuffer );
    rd->free_rid( textures_uniform_set );
}

void MFBackgroundEffect::_bind_methods()
{
}
