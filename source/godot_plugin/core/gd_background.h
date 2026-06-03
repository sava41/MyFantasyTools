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

class MFBackgroundEffect : public godot::CompositorEffect
{
    GDCLASS( MFBackgroundEffect, godot::CompositorEffect )

  protected:
    static void _bind_methods();

  public:
    MFBackgroundEffect();
    ~MFBackgroundEffect() override;

    void _render_callback( int effect_callback_type, godot::RenderData* render_data ) override;

    void set_view_textures( godot::Ref<godot::ImageTexture> color_direct,
                            godot::Ref<godot::ImageTexture> color_indirect,
                            godot::Ref<godot::ImageTexture> depth_baked );

    void set_view_params( float uncropped_fov,
                          float uncropped_aspect,
                          godot::Transform3D uncropped_view_mat );

  private:
    // std140-compatible UBO layout (must match background_composite.glsl Params block)
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

    void initialize( godot::RenderingDevice* rd );

    bool m_initialized = false;

    // View state (set by MFLevel::apply_view)
    godot::Ref<godot::ImageTexture> m_color_direct;
    godot::Ref<godot::ImageTexture> m_color_indirect;
    godot::Ref<godot::ImageTexture> m_depth_baked;
    float              m_uncropped_fov    = 1.0f;
    float              m_uncropped_aspect = 1.0f;
    godot::Transform3D m_uncropped_view_mat;

    // RD resources (lifetime = plugin lifetime)
    godot::RID m_shader;
    godot::RID m_pipeline;
    godot::RID m_sampler;
    godot::RID m_params_buffer;
};
