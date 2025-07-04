#include "level_manager.h"
#include "view_data.h"

#include <stdio.h>

class ViewDataSimple : public mft::ViewData
{
  public:
    using mft::ViewData::ViewData;

    bool load_camera_data( const std::array<float, MAT4_SIZE>& transform, int sizex, int sizey, float fov ) override
    {
        // Do nothing
        return true;
    }

    void* create_image_buffer( int sizex, int sizey, int channels, size_t bufferSize, const ImageType& type ) override
    {
        switch( type )
        {
        case mft::ViewData::Color:
            m_color.resize( bufferSize );
            return m_color.data();
        case mft::ViewData::Depth:
            m_depth.resize( bufferSize );
            return m_depth.data();
        case mft::ViewData::Environment:
            m_env.resize( bufferSize );
            return m_env.data();
        }

        return nullptr;
    }

    void destroy_image_buffers() override
    {
        m_color.clear();
        m_depth.clear();
        m_env.clear();
    }

  private:
    std::vector<char> m_color;
    std::vector<char> m_depth;
    std::vector<char> m_env;
};

std::unique_ptr<mft::ViewData> createViewDataSimple( const std::string& name, const std::filesystem::path& dataDir, unsigned int imageTypesFlags )
{
    return std::make_unique<ViewDataSimple>( name, dataDir, imageTypesFlags );
}

int main( int argc, char* argv[] )
{
    if( argc <= 1 )
    {
        fprintf( stderr, "Enter path to level" );
        return 1;
    }

    mft::LevelManager manager( createViewDataSimple );
    if( !manager.load_level( argv[1] ) )
    {
        return 1;
    }

    printf( "there are %d cameras in this scene\n", manager.get_views_length() );
    printf( "there are %d triangles in the navmesh\n", manager.get_navmesh_tris_length() );

    printf( "fini" );
    return 0;
}