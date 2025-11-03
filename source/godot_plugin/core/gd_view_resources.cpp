#include "gd_view_resources.h"

#include <cassert>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image_texture.hpp>

bool GDViewResources::load_camera_data()
{

    const auto* transform_ptr = m_view_info->world_transform();

    // Transform comes in as 1x16 vector representing 4x4 matrix
    // Godot transform constructor takes basis vectors first and offset vector last
    m_transform = godot::Transform3D( transform_ptr->m00(), transform_ptr->m01(), transform_ptr->m02(), transform_ptr->m10(), transform_ptr->m11(),
                                      transform_ptr->m12(), transform_ptr->m20(), transform_ptr->m21(), transform_ptr->m22(), transform_ptr->m30(),
                                      transform_ptr->m31(), transform_ptr->m32() );


    // Input tranform is z-up we need to convert to y-up for godot
    m_transform.rotate( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 );

    return true;
}

void* GDViewResources::create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type )
{
    assert( channels == 3 || channels == 1 );

    switch( type )
    {
    case ImageType::Color:
        m_colorBuffer = godot::Image::create( sizex, sizey, false, godot::Image::FORMAT_RGBF );
        return const_cast<uint8_t*>( m_colorBuffer->get_data().ptr() );

    case ImageType::Depth:
        m_depthBuffer = godot::Image::create( sizex, sizey, false, godot::Image::FORMAT_RF );
        return const_cast<uint8_t*>( m_depthBuffer->get_data().ptr() );

    case ImageType::Environment:
        m_envBuffer = godot::Image::create( sizex, sizey, false, godot::Image::FORMAT_RGBF );
        return const_cast<uint8_t*>( m_envBuffer->get_data().ptr() );
    }

    return nullptr;
}

void GDViewResources::destroy_image_buffers()
{
}