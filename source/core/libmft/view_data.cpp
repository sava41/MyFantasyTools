
#include "view_data.h"

#include "io.h"
#include "jxl.h"

namespace mft
{

    ViewData::ViewData( const std::string& name, const std::filesystem::path& dataDir, unsigned int imageTypesFlags )
        : m_name( name )
        , m_imageTypeFlags( imageTypesFlags )
    {
        for( const auto& type : ImageTypeStrings )
        {
            unsigned int typeInt = static_cast<unsigned int>( type.first );
            if( typeInt & imageTypesFlags )
            {
                m_imagePaths.insert( { type.first, dataDir / std::filesystem::path( name + "_" + type.second + ".jxl" ) } );
            }
        }
    }

    ViewData::~ViewData()
    {
        unload_image_data();
    }

    bool ViewData::load_image_data()
    {
        if( m_imagesLoaded )
            return false;

        for( auto& image : m_imagePaths )
        {
            std::vector<char> compressed = read_binary( image.second );
            bool res                     = jxl::decode_oneshot( compressed,
                                                                [this, &image]( int sizex, int sizey, int channels, size_t bufferSize ) -> void*
                                                                {
                                                void* bufferPtr = create_image_buffer( sizex, sizey, channels, image.first );

                                                m_imageBuffers.insert( { image.first, bufferPtr } );

                                                return bufferPtr;
                                            } );
            if( !res )
            {
                unload_image_data();
                return false;
            }
            m_imagesLoaded = m_imagesLoaded | static_cast<unsigned int>( image.first );
        }

        return true;
    }

    bool ViewData::unload_image_data()
    {
        destroy_image_buffers();

        m_imageBuffers.clear();

        m_imagesLoaded = 0;

        return true;
    }

    bool ViewData::is_data_loaded() const
    {
        return m_imagesLoaded == m_imageTypeFlags;
    }

} // namespace mft