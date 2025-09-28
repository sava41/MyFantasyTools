
#include "mf_view_data.h"

#include "io.h"
#include "jxl.h"

namespace mft
{

    ViewData::ViewData()
        : m_is_init( false )
        , m_images_loaded( false )
    {
    }

    ViewData::~ViewData()
    {
        unload_image_data();
    }

    void ViewData::init( const std::string& name, const std::filesystem::path& data_dir, unsigned int image_type_flags )
    {
        m_name             = name;
        m_image_type_flags = image_type_flags;

        for( const auto& type : ImageTypeStrings )
        {
            unsigned int type_int = static_cast<unsigned int>( type.first );
            if( type_int & image_type_flags )
            {
                m_image_paths.insert( { type.first, data_dir / std::filesystem::path( name + "_" + type.second + ".jxl" ) } );
            }
        }

        m_is_init = true;
    }

    bool ViewData::load_image_data()
    {
        if( m_images_loaded || !m_is_init )
            return false;

        for( auto& image : m_image_paths )
        {
            std::vector<char> compressed = read_binary( image.second );
            bool res                     = jxl::decode_oneshot( compressed,
                                                                [this, &image]( int sizex, int sizey, int channels, size_t buffer_size ) -> void*
                                                                {
                                                void* buffer_ptr = create_image_buffer( sizex, sizey, channels, buffer_size, image.first );

                                                m_image_buffers.insert( { image.first, buffer_ptr } );

                                                return buffer_ptr;
                                            } );
            if( !res )
            {
                unload_image_data();
                return false;
            }
            m_images_loaded = m_images_loaded | static_cast<unsigned int>( image.first );
        }

        return true;
    }

    bool ViewData::unload_image_data()
    {
        destroy_image_buffers();

        m_image_buffers.clear();

        m_images_loaded = 0;

        return true;
    }

    bool ViewData::is_data_loaded() const
    {
        return m_images_loaded == m_image_type_flags;
    }

} // namespace mft