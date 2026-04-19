
#pragma once

#include "level_generated.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

constexpr int MAT4_SIZE = 16;

namespace mft
{

    class ViewResources
    {
      public:
        enum ImageType : uint32_t
        {
            ColorDirect    = 1 << 0,
            ColorIndirect  = 1 << 1,
            Depth          = 1 << 2,
            Environment    = 1 << 3,
            LightDirection = 1 << 4
        };

        const std::unordered_map<ImageType, std::string> ImageTypeStrings{ { ColorDirect, "ColorDirect" }, { ColorIndirect, "ColorIndirect" }, { Depth, "Depth" }, { Environment, "Environment" }, { LightDirection, "LightDirection" } };

        ViewResources();
        ~ViewResources();

        ViewResources( const ViewResources& )  = delete;
        ViewResources( ViewResources&& other ) = delete;

        ViewResources& operator=( const ViewResources& ) = delete;
        ViewResources& operator=( ViewResources&& )      = delete;

        void init( const mft::data::View* view_info, const std::filesystem::path& data_dir );
        // Init for .mflevel: image data is read from an in-memory blob using
        // byte offsets stored in the FlatBuffer view_info.
        void init( const mft::data::View* view_info, const char* image_blob, size_t image_blob_size );

        bool is_data_loaded() const;
        bool load_image_data();
        bool unload_image_data();

        // Load camera data in the native engine camera class
        virtual bool load_camera_data() = 0;

        // Create an image container and return the pointer to the data buffer
        // ImageType is used to keep track which type of image buffer was created
        virtual void* create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type ) = 0;

        // Destroy and release memory for all the image containers;
        virtual void destroy_image_buffers() = 0;

      public:
        unsigned int m_image_type_flags;
        const data::View* m_view_info;

      private:
        std::unordered_map<ImageType, std::filesystem::path> m_image_paths;
        std::unordered_map<ImageType, void*> m_image_buffers;

        const char* m_image_blob_ptr  = nullptr; // non-null when loading from .mflevel
        size_t      m_image_blob_size = 0;

        std::atomic<unsigned int> m_is_init;
        std::atomic<unsigned int> m_images_loaded;
    };

} // namespace mft