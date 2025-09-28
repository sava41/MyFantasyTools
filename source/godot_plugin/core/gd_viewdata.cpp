#include "gd_viewdata.h"

#include <cassert>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image_texture.hpp>

bool MFViewData::load_camera_data( const mft::ViewData::CameraData& camera_data )
{

    m_fov = camera_data.fov;

    m_size_x = camera_data.size_x;
    m_size_y = camera_data.size_y;

    // Transform comes in as 1x16 vector representing 4x4 matrix
    // Godot transform constructor takes basis vectors first and offset vector last
    m_transform = godot::Transform3D( camera_data.transform[0], camera_data.transform[1], camera_data.transform[2], camera_data.transform[4],
                                      camera_data.transform[5], camera_data.transform[6], camera_data.transform[8], camera_data.transform[9],
                                      camera_data.transform[10], camera_data.transform[3], camera_data.transform[7], camera_data.transform[11] );

    // Input tranform is z-up we need to convert to y-up for godot
    m_transform.rotate( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 );

    return true;
}

void* MFViewData::create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type )
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

void MFViewData::destroy_image_buffers()
{
}