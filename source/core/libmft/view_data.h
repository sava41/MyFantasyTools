
#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

constexpr int MAT4_SIZE = 16;

namespace mft
{

    class ViewData
    {
      public:
        enum ImageType : uint32_t
        {
            Color       = 1 << 0,
            Depth       = 1 << 1,
            Environment = 1 << 2
        };

        const std::unordered_map<ImageType, std::string> ImageTypeStrings{ { Color, "Color" }, { Depth, "Depth" }, { Environment, "Environment" } };

        ViewData();
        ~ViewData();

        ViewData( const ViewData& )  = delete;
        ViewData( ViewData&& other ) = delete;

        ViewData& operator=( const ViewData& ) = delete;
        ViewData& operator=( ViewData&& )      = delete;

        void init( const std::string& name, const std::filesystem::path& data_dir, unsigned int image_type_flags );

        bool is_data_loaded() const;
        bool load_image_data();
        bool unload_image_data();

        // Load camera data in the native engine camera class
        virtual bool load_camera_data( const std::array<float, MAT4_SIZE>& transform, int sizex, int sizey, float fov ) = 0;

        // Create an image container and return the pointer to the data buffer
        // ImageType is used to keep track which type of image buffer was created
        virtual void* create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type ) = 0;

        // Destroy and release memory for all the image containers;
        virtual void destroy_image_buffers() = 0;

      public:
        std::string m_name;
        int m_id;
        int m_size_x;
        int m_size_y;

        unsigned int m_image_type_flags;

      private:
        std::unordered_map<ImageType, std::filesystem::path> m_image_paths;
        std::unordered_map<ImageType, void*> m_image_buffers;

        std::atomic<unsigned int> m_is_init;
        std::atomic<unsigned int> m_images_loaded;
    };

} // namespace mft