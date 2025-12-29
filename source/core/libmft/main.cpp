#include "mf_view_level_manager.h"
#include "mf_view_resources.h"

#include <stdio.h>

class ViewResourcesSimple : public mft::ViewResources
{
  public:
    bool load_camera_data() override
    {
        // Do nothing
        return true;
    }

    void* create_image_buffer( int sizex, int sizey, int channels, size_t buffer_size, const ImageType& type ) override
    {
        switch( type )
        {
        case mft::ViewResources::ColorDirect:
            m_color.resize( buffer_size );
            return m_color.data();
        case mft::ViewResources::ColorIndirect:
            return nullptr;
        case mft::ViewResources::Depth:
            m_depth.resize( buffer_size );
            return m_depth.data();
        case mft::ViewResources::Environment:
            m_env.resize( buffer_size );
            return m_env.data();
        case mft::ViewResources::LightDirection:
            m_light_dir.resize( buffer_size );
            return m_light_dir.data();
        }


        return nullptr;
    }

    void destroy_image_buffers() override
    {
        m_color.clear();
        m_depth.clear();
        m_env.clear();
        m_light_dir.clear();
    }

  private:
    std::vector<char> m_color;
    std::vector<char> m_depth;
    std::vector<char> m_env;
    std::vector<char> m_light_dir;
};

int main( int argc, char* argv[] )
{
    if( argc <= 1 )
    {
        fprintf( stderr, "Enter path to level" );
        return 1;
    }

    mft::ViewLevelManager<ViewResourcesSimple> manager;
    if( !manager.load_data( argv[1] ) )
    {
        return 1;
    }

    printf( "there are %d cameras in this scene\n", manager.get_num_views() );
    printf( "there are %d triangles in the navmesh\n", manager.get_navmesh_num_tris() );

    printf( "fini" );
    return 0;
}