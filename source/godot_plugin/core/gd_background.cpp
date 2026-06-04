#include "gd_background.h"

#include <godot_cpp/classes/rd_pipeline_color_blend_state.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state_attachment.hpp>
#include <godot_cpp/classes/rd_pipeline_depth_stencil_state.hpp>
#include <godot_cpp/classes/rd_pipeline_multisample_state.hpp>
#include <godot_cpp/classes/rd_pipeline_rasterization_state.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
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
    m_params_bytes.resize( sizeof( BackgroundParams ) );
}

static inline void free_if_valid( godot::RenderingDevice* rd, godot::RID& rid )
{
    if( rid.is_valid() )
    {
        rd->free_rid( rid );
        rid = godot::RID();
    }
}

MFBackgroundEffect::~MFBackgroundEffect()
{
    godot::RenderingDevice* rd = godot::RenderingServer::get_singleton()->get_rendering_device();
    if( !rd )
        return;

    free_if_valid( rd, m_bg_depth_texture );
    free_if_valid( rd, m_ssao_texture );
    free_if_valid( rd, m_beauty_texture );
    free_if_valid( rd, m_stage2_params_uniform_set );
    free_if_valid( rd, m_stage1_params_uniform_set );
    free_if_valid( rd, m_params_buffer );
    free_if_valid( rd, m_sampler );
    free_if_valid( rd, m_vertex_array );
    free_if_valid( rd, m_vertex_buffer );
    free_if_valid( rd, m_stage1_pipeline );
    free_if_valid( rd, m_stage1_shader );
    free_if_valid( rd, m_stage2_pipeline );
    free_if_valid( rd, m_stage2_shader );
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

// ---- Helpers ----------------------------------------------------------------

static godot::RID load_shader( godot::RenderingDevice* rd, const char* path )
{
    godot::Ref<godot::RDShaderFile> file = godot::ResourceLoader::get_singleton()->load( path );
    if( !file.is_valid() )
    {
        godot::UtilityFunctions::printerr( godot::String( "MFBackgroundEffect: could not load " ) + path );
        return godot::RID();
    }
    godot::Ref<godot::RDShaderSPIRV> spirv = file->get_spirv();
    if( !spirv.is_valid() )
    {
        godot::UtilityFunctions::printerr( godot::String( "MFBackgroundEffect: no SPIRV in " ) + path );
        return godot::RID();
    }
    return rd->shader_create_from_spirv( spirv );
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

// ---- Initialization ---------------------------------------------------------

void MFBackgroundEffect::init( godot::RenderingDevice* rd, godot::RID color_tex, godot::RID depth_tex )
{
    // ---- Shaders ----
    m_stage1_shader = load_shader( rd, "res://addons/mft_godot_plugin/background.glsl" );
    m_stage2_shader = load_shader( rd, "res://addons/mft_godot_plugin/background_composite.glsl" );

    if( !m_stage1_shader.is_valid() || !m_stage2_shader.is_valid() )
        return;

    // ---- Shared sampler ----
    godot::Ref<godot::RDSamplerState> ss;
    ss.instantiate();
    ss->set_min_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_mag_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_mip_filter( godot::RenderingDevice::SAMPLER_FILTER_LINEAR );
    ss->set_repeat_u( godot::RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE );
    ss->set_repeat_v( godot::RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE );
    m_sampler = rd->sampler_create( ss );

    // ---- Params UBO (set 0) — one per shader due to differing stage visibility ----
    m_params_buffer = rd->uniform_buffer_create( sizeof( BackgroundParams ) );

    godot::Ref<godot::RDUniform> u_params;
    u_params.instantiate();
    u_params->set_uniform_type( godot::RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER );
    u_params->set_binding( 0 );
    u_params->add_id( m_params_buffer );
    godot::TypedArray<godot::RDUniform> params_arr;
    params_arr.push_back( u_params );
    m_stage1_params_uniform_set = rd->uniform_set_create( params_arr, m_stage1_shader, 0 );
    m_stage2_params_uniform_set = rd->uniform_set_create( params_arr, m_stage2_shader, 0 );

    // ---- Fullscreen quad ----
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
    m_vertex_format = rd->vertex_format_create( vertex_attrs );

    godot::TypedArray<godot::RID> vertex_bufs;
    vertex_bufs.push_back( m_vertex_buffer );
    godot::PackedInt64Array vertex_offsets;
    vertex_offsets.push_back( 0 );
    m_vertex_array = rd->vertex_array_create( 6, m_vertex_format, vertex_bufs, vertex_offsets );

    // ---- Stage 2 framebuffer format (color + depth) ----
    {
        godot::TypedArray<godot::RID> probe_atts;
        probe_atts.push_back( color_tex );
        probe_atts.push_back( depth_tex );
        godot::RID probe_fb = rd->framebuffer_create( probe_atts );
        m_stage2_fb_format  = rd->framebuffer_get_format( probe_fb );
        rd->free_rid( probe_fb );
    }

    // ---- Stage 2 render pipeline (depth test GREATER + depth write) ----
    {
        godot::Ref<godot::RDPipelineRasterizationState> rast;
        rast.instantiate();
        godot::Ref<godot::RDPipelineMultisampleState> ms;
        ms.instantiate();

        godot::Ref<godot::RDPipelineDepthStencilState> ds;
        ds.instantiate();
        ds->set_enable_depth_test( true );
        ds->set_enable_depth_write( true );
        ds->set_depth_compare_operator( godot::RenderingDevice::COMPARE_OP_GREATER );

        godot::Ref<godot::RDPipelineColorBlendStateAttachment> ba;
        ba.instantiate();
        godot::TypedArray<godot::RDPipelineColorBlendStateAttachment> blend_atts;
        blend_atts.push_back( ba );
        godot::Ref<godot::RDPipelineColorBlendState> blend;
        blend.instantiate();
        blend->set_attachments( blend_atts );

        m_stage2_pipeline = rd->render_pipeline_create( m_stage2_shader, m_stage2_fb_format, m_vertex_format,
                                                        godot::RenderingDevice::RENDER_PRIMITIVE_TRIANGLES, rast, ms, ds, blend );
    }

    // Stage 1 pipeline is deferred to ensure_intermediate_textures (needs fb format
    // from the actual intermediate texture dimensions which aren't known yet).
}

void MFBackgroundEffect::create_render_textures( godot::RenderingDevice* rd, godot::Vector2i size )
{
    if( m_beauty_texture.is_valid() )
        rd->free_rid( m_beauty_texture );
    if( m_ssao_texture.is_valid() )
        rd->free_rid( m_ssao_texture );
    if( m_bg_depth_texture.is_valid() )
        rd->free_rid( m_bg_depth_texture );
    if( m_stage1_pipeline.is_valid() )
        rd->free_rid( m_stage1_pipeline );

    // RGBA16F for beauty and ssao
    godot::Ref<godot::RDTextureFormat> rgba_fmt;
    rgba_fmt.instantiate();
    rgba_fmt->set_format( godot::RenderingDevice::DATA_FORMAT_R16G16B16A16_SFLOAT );
    rgba_fmt->set_width( size.x );
    rgba_fmt->set_height( size.y );
    rgba_fmt->set_depth( 1 );
    rgba_fmt->set_array_layers( 1 );
    rgba_fmt->set_mipmaps( 1 );
    rgba_fmt->set_texture_type( godot::RenderingDevice::TEXTURE_TYPE_2D );
    rgba_fmt->set_usage_bits( godot::RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | godot::RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
                              godot::RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT );

    // R32F for bg_depth (higher precision needed for clip-space depth values)
    godot::Ref<godot::RDTextureFormat> r32_fmt;
    r32_fmt.instantiate();
    r32_fmt->set_format( godot::RenderingDevice::DATA_FORMAT_R32_SFLOAT );
    r32_fmt->set_width( size.x );
    r32_fmt->set_height( size.y );
    r32_fmt->set_depth( 1 );
    r32_fmt->set_array_layers( 1 );
    r32_fmt->set_mipmaps( 1 );
    r32_fmt->set_texture_type( godot::RenderingDevice::TEXTURE_TYPE_2D );
    r32_fmt->set_usage_bits( godot::RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | godot::RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
                             godot::RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT );

    godot::Ref<godot::RDTextureView> view;
    view.instantiate();

    m_beauty_texture   = rd->texture_create( rgba_fmt, view );
    m_ssao_texture     = rd->texture_create( rgba_fmt, view );
    m_bg_depth_texture = rd->texture_create( r32_fmt, view );

    // Probe Stage 1 framebuffer format from the actual intermediate textures
    {
        godot::TypedArray<godot::RID> probe_atts;
        probe_atts.push_back( m_beauty_texture );
        probe_atts.push_back( m_ssao_texture );
        probe_atts.push_back( m_bg_depth_texture );
        godot::RID probe_fb = rd->framebuffer_create( probe_atts );
        m_stage1_fb_format  = rd->framebuffer_get_format( probe_fb );
        rd->free_rid( probe_fb );
    }

    // Stage 1 pipeline: three color outputs, no depth test/write
    godot::Ref<godot::RDPipelineRasterizationState> rast;
    rast.instantiate();
    godot::Ref<godot::RDPipelineMultisampleState> ms;
    ms.instantiate();
    godot::Ref<godot::RDPipelineDepthStencilState> ds;
    ds.instantiate();

    godot::Ref<godot::RDPipelineColorBlendStateAttachment> ba0;
    ba0.instantiate();
    godot::Ref<godot::RDPipelineColorBlendStateAttachment> ba1;
    ba1.instantiate();
    godot::Ref<godot::RDPipelineColorBlendStateAttachment> ba2;
    ba2.instantiate();
    godot::TypedArray<godot::RDPipelineColorBlendStateAttachment> blend_atts;
    blend_atts.push_back( ba0 );
    blend_atts.push_back( ba1 );
    blend_atts.push_back( ba2 );
    godot::Ref<godot::RDPipelineColorBlendState> blend;
    blend.instantiate();
    blend->set_attachments( blend_atts );

    m_stage1_pipeline = rd->render_pipeline_create( m_stage1_shader, m_stage1_fb_format, m_vertex_format, godot::RenderingDevice::RENDER_PRIMITIVE_TRIANGLES,
                                                    rast, ms, ds, blend );
}

// ---- Render callback --------------------------------------------------------

void MFBackgroundEffect::_render_callback( int /*type*/, godot::RenderData* render_data )
{
    if( !m_color_direct.is_valid() || !m_color_indirect.is_valid() || !m_depth_baked.is_valid() )
        return;

    assert( render_data );
    godot::RenderingDevice* rd = godot::RenderingServer::get_singleton()->get_rendering_device();
    assert( rd );

    godot::Ref<godot::RenderSceneBuffersRD> buffers = render_data->get_render_scene_buffers();
    assert( buffers.is_valid() );

    godot::Vector2i size = buffers->get_internal_size();
    assert( size.x > 0 && size.y > 0 );

    godot::RID color_tex = buffers->get_color_texture();
    godot::RID depth_tex = buffers->get_depth_texture();

    if( !m_initialized )
    {
        init( rd, color_tex, depth_tex );
        m_initialized = true;
    }

    if( m_intermediate_size != size )
    {
        create_render_textures( rd, size );
        m_intermediate_size = size;
    }

    // ---- Upload BackgroundParams ----
    godot::RenderSceneData* scene_data = render_data->get_render_scene_data();
    godot::Transform3D cam_transform   = scene_data->get_cam_transform();
    godot::Projection cam_proj         = scene_data->get_cam_projection();

    BackgroundParams& p = *reinterpret_cast<BackgroundParams*>( m_params_bytes.ptrw() );
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
    rd->buffer_update( m_params_buffer, 0, m_params_bytes.size(), m_params_bytes );

    // ---- Get RD RIDs for baked textures ----
    godot::RenderingServer* rs   = godot::RenderingServer::get_singleton();
    godot::RID color_direct_rd   = rs->texture_get_rd_texture( m_color_direct->get_rid() );
    godot::RID color_indirect_rd = rs->texture_get_rd_texture( m_color_indirect->get_rid() );
    godot::RID depth_baked_rd    = rs->texture_get_rd_texture( m_depth_baked->get_rid() );

    // =========================================================================
    // Stage 1 — background.glsl: beauty + ssao + bg_depth outputs
    // =========================================================================
    {
        godot::TypedArray<godot::RDUniform> set1;
        set1.push_back( make_sampled( 0, color_direct_rd, m_sampler ) );
        set1.push_back( make_sampled( 1, color_indirect_rd, m_sampler ) );
        set1.push_back( make_sampled( 2, depth_baked_rd, m_sampler ) );
        set1.push_back( make_sampled( 3, depth_tex, m_sampler ) );
        godot::RID stage1_tex_set = rd->uniform_set_create( set1, m_stage1_shader, 1 );

        godot::TypedArray<godot::RID> fb_atts;
        fb_atts.push_back( m_beauty_texture );
        fb_atts.push_back( m_ssao_texture );
        fb_atts.push_back( m_bg_depth_texture );
        godot::RID stage1_fb = rd->framebuffer_create( fb_atts, m_stage1_fb_format );

        // Clear ssao to ao=1.0 (no darkening at discarded/occluded pixels)
        // Clear bg_depth to 0.0 (no background → Stage 2 depth test won't pass)
        godot::PackedColorArray clear_colors;
        clear_colors.push_back( godot::Color( 0, 0, 0, 0 ) ); // beauty: don't care
        clear_colors.push_back( godot::Color( 1, 0, 0, 1 ) ); // ssao: ao=1
        clear_colors.push_back( godot::Color( 0, 0, 0, 0 ) ); // bg_depth: 0 = no background

        int64_t dl = rd->draw_list_begin( stage1_fb, godot::RenderingDevice::DRAW_CLEAR_COLOR_1 | godot::RenderingDevice::DRAW_CLEAR_COLOR_2, clear_colors );
        rd->draw_list_bind_render_pipeline( dl, m_stage1_pipeline );
        rd->draw_list_bind_uniform_set( dl, m_stage1_params_uniform_set, 0 );
        rd->draw_list_bind_uniform_set( dl, stage1_tex_set, 1 );
        rd->draw_list_bind_vertex_array( dl, m_vertex_array );
        rd->draw_list_draw( dl, false, 1 );
        rd->draw_list_end();

        rd->free_rid( stage1_fb );
        rd->free_rid( stage1_tex_set );
    }

    // =========================================================================
    // Stage 2 — background_composite.glsl: beauty → scene color, depth write
    // =========================================================================
    {
        godot::TypedArray<godot::RDUniform> set1;
        set1.push_back( make_sampled( 0, m_beauty_texture, m_sampler ) );
        set1.push_back( make_sampled( 1, m_bg_depth_texture, m_sampler ) );
        godot::RID composite_set = rd->uniform_set_create( set1, m_stage2_shader, 1 );

        godot::TypedArray<godot::RID> fb_atts;
        fb_atts.push_back( color_tex );
        fb_atts.push_back( depth_tex );
        godot::RID framebuffer = rd->framebuffer_create( fb_atts, m_stage2_fb_format );

        int64_t dl = rd->draw_list_begin( framebuffer );
        rd->draw_list_bind_render_pipeline( dl, m_stage2_pipeline );
        rd->draw_list_bind_uniform_set( dl, m_stage2_params_uniform_set, 0 );
        rd->draw_list_bind_uniform_set( dl, composite_set, 1 );
        rd->draw_list_bind_vertex_array( dl, m_vertex_array );
        rd->draw_list_draw( dl, false, 1 );
        rd->draw_list_end();

        rd->free_rid( framebuffer );
        rd->free_rid( composite_set );
    }
}

void MFBackgroundEffect::_bind_methods()
{
}
