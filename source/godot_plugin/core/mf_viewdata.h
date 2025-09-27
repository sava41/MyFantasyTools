#pragma once

#include <godot_cpp/classes/image.hpp>
#include <view_data.h>

class MFViewData : public mft::ViewData
{
  public:
    MFViewData()          = default;
    virtual ~MFViewData() = default;

    virtual bool load_camera_data( const mft::ViewData::CameraData& camera_data ) override;

    virtual void* create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type ) override;

    virtual void destroy_image_buffers() override;

    MFViewData& get_view_data( int index );

    godot::Transform3D m_transform;
    float m_fov;

    godot::Ref<godot::Image> m_colorBuffer;
    godot::Ref<godot::Image> m_depthBuffer;
    godot::Ref<godot::Image> m_envBuffer;
};