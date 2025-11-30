#pragma once

#include <godot_cpp/classes/image.hpp>
#include <mf_view_resources.h>

class GDViewResources : public mft::ViewResources
{
  public:
    GDViewResources()          = default;
    virtual ~GDViewResources() = default;

    virtual bool load_camera_data() override;

    virtual void* create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type ) override;

    virtual void destroy_image_buffers() override;

    godot::Transform3D m_transform;

    godot::Ref<godot::Image> m_colorDirectBuffer;
    godot::Ref<godot::Image> m_colorIndirectBuffer;
    godot::Ref<godot::Image> m_depthBuffer;
    godot::Ref<godot::Image> m_envBuffer;
    godot::Ref<godot::Image> m_lightDirectionBuffer;
};