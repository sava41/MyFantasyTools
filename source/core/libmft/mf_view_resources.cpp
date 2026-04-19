
#include "mf_view_resources.h"

#include "io.h"
#include "jxl.h"

namespace mft
{

    ViewResources::ViewResources()
        : m_is_init( false )
        , m_images_loaded( false )
        , m_view_info( nullptr )
    {
    }

    ViewResources::~ViewResources()
    {
        unload_image_data();
    }

    void ViewResources::init( const mft::data::View* view_info, const std::filesystem::path& data_dir )
    {
        m_view_info       = view_info;
        m_image_blob_ptr  = nullptr;
        m_image_blob_size = 0;

        // TODO: for now views have hardcoded channels
        m_image_type_flags = ColorDirect | ColorIndirect | Depth | Environment | LightDirection;

        std::string name = view_info->name()->c_str();

        for( const auto& type : ImageTypeStrings )
        {
            unsigned int type_int = static_cast<unsigned int>( type.first );
            if( type_int & m_image_type_flags )
            {
                m_image_paths.insert( { type.first, data_dir / std::filesystem::path( name + "_" + type.second + ".jxl" ) } );
            }
        }

        m_is_init = true;
    }

    void ViewResources::init( const mft::data::View* view_info, const char* image_blob, size_t image_blob_size )
    {
        m_view_info       = view_info;
        m_image_blob_ptr  = image_blob;
        m_image_blob_size = image_blob_size;

        // TODO: for now views have hardcoded channels
        m_image_type_flags = ColorDirect | ColorIndirect | Depth | Environment | LightDirection;

        m_is_init = true;
    }

    bool ViewResources::load_image_data()
    {
        if( m_images_loaded || !m_is_init )
            return false;

        if( m_image_blob_ptr )
        {
            // .mflevel path: read each image from the in-memory blob using offsets
            // stored in the FlatBuffer view entry.
            auto load_image = [&]( ImageType type, const data::ImageEntry* entry ) -> bool
            {
                if( !entry )
                    return true; // field absent, skip

                const auto offset = static_cast<size_t>( entry->offset() );
                const auto size   = static_cast<size_t>( entry->size() );

                if( offset + size > m_image_blob_size )
                {
                    fprintf( stderr, "Image entry out of bounds for view '%s'\n", m_view_info->name()->c_str() );
                    return false;
                }

                std::vector<char> compressed( m_image_blob_ptr + offset, m_image_blob_ptr + offset + size );

                bool res = jxl::decode_oneshot( compressed,
                    [this, type]( int sizex, int sizey, int channels, size_t buffer_size ) -> void*
                    {
                        void* ptr = create_image_buffer( sizex, sizey, channels, buffer_size, type );
                        m_image_buffers.insert( { type, ptr } );
                        return ptr;
                    } );

                if( res )
                    m_images_loaded = m_images_loaded | static_cast<unsigned int>( type );

                return res;
            };

            bool res = true;
            res = res && load_image( ColorDirect,    m_view_info->color_direct() );
            res = res && load_image( ColorIndirect,  m_view_info->color_indirect() );
            res = res && load_image( Depth,          m_view_info->depth() );
            res = res && load_image( Environment,    m_view_info->environment() );
            res = res && load_image( LightDirection, m_view_info->light_direction() );

            if( !res )
            {
                unload_image_data();
                return false;
            }
        }
        else
        {
            // .level path: read each image from the filesystem.
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
        }

        return true;
    }

    bool ViewResources::unload_image_data()
    {
        destroy_image_buffers();

        m_image_buffers.clear();

        m_images_loaded = 0;

        return true;
    }

    bool ViewResources::is_data_loaded() const
    {
        return m_images_loaded == m_image_type_flags;
    }

} // namespace mft