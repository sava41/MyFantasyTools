#pragma once

#include <godot_cpp/classes/image.hpp>
#include <view_data.h>

class MFViewData : public mft::ViewData
{
  public:
    virtual bool load_camera_data( const std::array<float, MAT4_SIZE>& transform, int sizex, int sizey, float fov ) override;

    virtual void* create_image_buffer( int sizex, int sizey, int channels, const ImageType& type ) override;

    virtual void destroy_image_buffers() override;

    MFViewData& get_view_data( int index );

    godot::Transform3D m_transform;
    float m_fov;

    godot::Ref<godot::Image> m_colorBuffer;
    godot::Ref<godot::Image> m_depthBuffer;
    godot::Ref<godot::Image> m_envBuffer;
};