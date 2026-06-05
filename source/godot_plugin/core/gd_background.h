#pragma once

#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/render_scene_data.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>

class MFBackgroundEffect : public godot::CompositorEffect
{
    GDCLASS( MFBackgroundEffect, godot::CompositorEffect )

  protected:
    static void _bind_methods();

  public:
    MFBackgroundEffect();
    ~MFBackgroundEffect() override;

    void _render_callback( int effect_callback_type, godot::RenderData* render_data ) override;

    void set_view_textures( godot::Ref<godot::ImageTexture> direct_diffuse,
                            godot::Ref<godot::ImageTexture> direct_specular,
                            godot::Ref<godot::ImageTexture> indirect_diffuse,
                            godot::Ref<godot::ImageTexture> indirect_specular,
                            godot::Ref<godot::ImageTexture> normal_baked,
                            godot::Ref<godot::ImageTexture> depth_baked );

    void set_view_params( float uncropped_fov, float uncropped_aspect, godot::Transform3D uncropped_view_mat );

  private:
    // std140-compatible UBO layout (must match Params block in all shaders)
    struct alignas( 16 ) BackgroundParams
    {
        float inv_projection[16];
        float projection[16];
        float view_matrix[16];
        float inv_view_matrix[16];
        float uncropped_view_mat[16];
        float inv_uncropped_view_mat[16];
        float uncropped_fov;
        float uncropped_aspect;
        int32_t screen_w;
        int32_t screen_h;
    };

    static void pack_projection( const godot::Projection& p, float* out );
    static void pack_transform( const godot::Transform3D& t, float* out );

    void init( godot::RenderingDevice* rd, godot::RID color_tex, godot::RID depth_tex );
    void create_render_textures( godot::RenderingDevice* rd, godot::Vector2i size );

    bool m_initialized = false;
    godot::PackedByteArray m_params_bytes;

    // View state (set by MFLevel::apply_view)
    godot::Ref<godot::ImageTexture> m_direct_diffuse;
    godot::Ref<godot::ImageTexture> m_direct_specular;
    godot::Ref<godot::ImageTexture> m_indirect_diffuse;
    godot::Ref<godot::ImageTexture> m_indirect_specular;
    godot::Ref<godot::ImageTexture> m_normal_baked;
    godot::Ref<godot::ImageTexture> m_depth_baked;
    float m_uncropped_fov    = 1.0f;
    float m_uncropped_aspect = 1.0f;
    godot::Transform3D m_uncropped_view_mat;

    // Shared RD resources (lifetime = plugin lifetime)
    godot::RID m_sampler;
    godot::RID m_params_buffer;
    godot::RID m_stage1_params_uniform_set; // set 0 for stage 1 (vertex+fragment layout)
    godot::RID m_stage2_params_uniform_set; // set 0 for stage 2 (fragment-only layout)
    godot::RID m_vertex_buffer;
    godot::RID m_vertex_array;
    int64_t m_vertex_format = -1;

    // Stage 1 — background render (beauty + ssao color outputs, no depth write)
    godot::RID m_stage1_shader;
    godot::RID m_stage1_pipeline;
    int64_t m_stage1_fb_format = -1; // probed from [beauty_tex, ssao_tex]

    // Stage 2 — final composite render (depth write)
    godot::RID m_stage2_shader;
    godot::RID m_stage2_pipeline;
    int64_t m_stage2_fb_format = -1; // probed from [color_tex, depth_tex]

    // Intermediate viewport-sized textures (reallocated on viewport resize)
    godot::RID m_beauty_texture;
    godot::RID m_ssao_texture;
    godot::RID m_bg_depth_texture; // R32F clip-space depth from Stage 1
    godot::Vector2i m_intermediate_size;
};
