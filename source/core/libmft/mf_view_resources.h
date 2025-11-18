
#pragma once

#include "level_generated.h"

#include <array>
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
            Color          = 1 << 0,
            Depth          = 1 << 1,
            Environment    = 1 << 2,
            LightDirection = 1 << 3
        };

        const std::unordered_map<ImageType, std::string> ImageTypeStrings{ { Color, "Color" }, { Depth, "Depth" }, { Environment, "Environment" }, { LightDirection, "LightDirection" } };

        ViewResources();
        ~ViewResources();

        ViewResources( const ViewResources& )  = delete;
        ViewResources( ViewResources&& other ) = delete;

        ViewResources& operator=( const ViewResources& ) = delete;
        ViewResources& operator=( ViewResources&& )      = delete;

        void init( const mft::data::View* view_info, const std::filesystem::path& data_dir );

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

        std::atomic<unsigned int> m_is_init;
        std::atomic<unsigned int> m_images_loaded;
    };

} // namespace mft