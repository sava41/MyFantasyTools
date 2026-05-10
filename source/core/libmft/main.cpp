#include "jxl.h"
#include "mf_level.h"

#include <stdio.h>
#include <vector>

int main( int argc, char* argv[] )
{
    if( argc <= 1 )
    {
        fprintf( stderr, "Usage: mftest <path-to.mflevel>\n" );
        return 1;
    }

    mft::Level level;
    if( !mft::load_level( argv[1], level ) )
    {
        fprintf( stderr, "Failed to load level\n" );
        return 1;
    }

    const auto* fbs     = level.fbs();
    const int num_views = static_cast<int>( fbs->views()->size() );

    printf( "Level:         %s\n", fbs->name()->c_str() );
    printf( "Views:         %d\n", num_views );
    printf( "Navmesh tris:  %d\n", mft::get_navmesh_num_tris( level ) );

    if( num_views == 0 )
    {
        printf( "no views\n" );
        return 0;
    }

    // Print the adjacency graph
    for( int i = 0; i < num_views; ++i )
    {
        const auto* view = fbs->views()->Get( i );
        printf( "  view %d (%s): adjacent ->", i, view->name()->c_str() );
        for( auto adj : *view->adjacent_views() )
            printf( " %d", adj );
        printf( "\n" );
    }

    // Test BFS from view 0
    const int hops = 2;
    auto in_range  = mft::get_views_in_range( level, 0, hops );
    printf( "\nViews within %d hops of view 0:", hops );
    for( int v : in_range )
        printf( " %d", v );
    printf( "\n" );

    // Load and decode the color_direct image for every view, verify no errors
    printf( "\nDecoding color_direct for all views...\n" );
    for( int i = 0; i < num_views; ++i )
    {
        const auto* view  = fbs->views()->Get( i );
        const auto* entry = view->color_direct();
        if( !entry )
        {
            printf( "  view %d: no color_direct entry\n", i );
            continue;
        }

        const int width               = view->res_x();
        const int height              = view->res_y();
        const size_t compressed_bytes = static_cast<size_t>( entry->size() );
        const size_t pixel_bytes      = static_cast<size_t>( width ) * height * 3 * sizeof( float );

        std::vector<char> compressed( compressed_bytes );
        std::vector<float> pixels( static_cast<size_t>( width ) * height * 3 );

        if( !mft::read_image_data( level, i, mft::ImageType::ColorDirect, compressed.data(), compressed_bytes ) )
        {
            printf( "  view %d: read_image_data failed\n", i );
            continue;
        }

        bool decode_ok = mft::jxl::decode_jxl( compressed.data(), compressed_bytes, pixels.data(), pixel_bytes );
        printf( "  view %d (%dx%d): %s\n", i, width, height, decode_ok ? "ok" : "FAILED" );
    }

    printf( "\nfini\n" );
    return 0;
}
