#include "io.h"
#include "jxl.h"
#include "mf_level.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace data = mft::data;

// --------------------------------------------------------------------------
// Image-type table shared by all modes
// --------------------------------------------------------------------------

static constexpr int NUM_IMAGE_TYPES = 5;

static const char* IMAGE_TYPE_NAMES[NUM_IMAGE_TYPES] = {
    "ColorDirect", "ColorIndirect", "Depth", "Environment", "LightDirection",
};

static const mft::ImageType IMAGE_TYPES[NUM_IMAGE_TYPES] = {
    mft::ImageType::ColorDirect,
    mft::ImageType::ColorIndirect,
    mft::ImageType::Depth,
    mft::ImageType::Environment,
    mft::ImageType::LightDirection,
};

static const data::ImageEntry* get_fbs_entry( const data::View* view, mft::ImageType type )
{
    switch( type )
    {
    case mft::ImageType::ColorDirect:    return view->color_direct();
    case mft::ImageType::ColorIndirect:  return view->color_indirect();
    case mft::ImageType::Depth:          return view->depth();
    case mft::ImageType::Environment:    return view->environment();
    case mft::ImageType::LightDirection: return view->light_direction();
    }
    return nullptr;
}

// --------------------------------------------------------------------------
// FlatBuffer in-place mutation helpers (no --gen-mutable needed)
//
// FlatBuffers vtable layout (all values little-endian):
//   vtable[0..1]: vtable byte size
//   vtable[2..3]: object data byte size
//   vtable[VT_*]: byte offset from table start to field (0 = field absent)
//   table[0..3]:  soffset_t pointing BACKWARDS to the vtable
//   table[fld]:   the field value
// --------------------------------------------------------------------------

static void patch_uint64( uint8_t* table_data, const uint8_t* vtable,
                           flatbuffers::voffset_t vtsize, flatbuffers::voffset_t vt_off, uint64_t val )
{
    if( vt_off >= vtsize )
        return;
    auto fld = flatbuffers::ReadScalar<flatbuffers::voffset_t>( vtable + vt_off );
    if( fld == 0 )
        return;
    flatbuffers::WriteScalar<uint64_t>( table_data + fld, val );
}

static void mutate_image_entry( const data::ImageEntry* entry, uint64_t new_offset, uint64_t new_size )
{
    auto* table_data      = reinterpret_cast<uint8_t*>( const_cast<data::ImageEntry*>( entry ) );
    auto  soffset         = flatbuffers::ReadScalar<flatbuffers::soffset_t>( table_data );
    const uint8_t* vtable = table_data - soffset;
    auto  vtsize          = flatbuffers::ReadScalar<flatbuffers::voffset_t>( vtable );
    patch_uint64( table_data, vtable, vtsize, data::ImageEntry::VT_OFFSET, new_offset );
    patch_uint64( table_data, vtable, vtsize, data::ImageEntry::VT_SIZE, new_size );
}

// --------------------------------------------------------------------------
// Inspect
// --------------------------------------------------------------------------

static int cmd_inspect( const char* path )
{
    mft::Level level;
    if( !mft::load_level( path, level ) )
    {
        fprintf( stderr, "Failed to load level\n" );
        return 1;
    }

    const auto* fbs     = level.fbs();
    const int num_views = static_cast<int>( fbs->views()->size() );

    printf( "Level:         %s\n", fbs->name()->c_str() );
    printf( "UUID:          %s\n", fbs->uuid() ? fbs->uuid()->c_str() : "(none)" );
    printf( "Views:         %d\n", num_views );
    printf( "Navmesh tris:  %d\n", mft::get_navmesh_num_tris( level ) );

    if( num_views == 0 )
        return 0;

    // Adjacency graph
    printf( "\n" );
    for( int i = 0; i < num_views; ++i )
    {
        const auto* view = fbs->views()->Get( i );
        printf( "  view %d (%s): adjacent ->", i, view->name()->c_str() );
        for( auto adj : *view->adjacent_views() )
            printf( " %d", adj );
        printf( "\n" );
    }

    // BFS test
    const int hops = 2;
    auto in_range  = mft::get_views_in_range( level, 0, hops );
    printf( "\nViews within %d hops of view 0:", hops );
    for( int v : in_range )
        printf( " %d", v );
    printf( "\n" );

    // Decode all image types for all views
    printf( "\nDecoding images for all views...\n" );
    for( int i = 0; i < num_views; ++i )
    {
        const auto* view = fbs->views()->Get( i );
        printf( "  view %d (%s):\n", i, view->name()->c_str() );

        for( int t = 0; t < NUM_IMAGE_TYPES; ++t )
        {
            const auto* entry = get_fbs_entry( view, IMAGE_TYPES[t] );
            if( !entry || entry->size() == 0 )
            {
                printf( "    %-20s  (no entry)\n", IMAGE_TYPE_NAMES[t] );
                continue;
            }

            const size_t compressed_bytes = static_cast<size_t>( entry->size() );
            const size_t pixel_bytes      = static_cast<size_t>( entry->res_x() ) * entry->res_y() * entry->channels() * sizeof( float );

            std::vector<char> compressed( compressed_bytes );
            std::vector<float> pixels( static_cast<size_t>( entry->res_x() ) * entry->res_y() * entry->channels() );

            if( !mft::read_image_data( level, i, IMAGE_TYPES[t], compressed.data(), compressed_bytes ) )
            {
                printf( "    %-20s  (read failed)\n", IMAGE_TYPE_NAMES[t] );
                continue;
            }

            bool ok = mft::jxl::decode_jxl( compressed.data(), compressed_bytes, pixels.data(), pixel_bytes );
            printf( "    %-20s  %5ux%-5u  ch=%u  %s\n",
                    IMAGE_TYPE_NAMES[t], entry->res_x(), entry->res_y(), entry->channels(),
                    ok ? "ok" : "DECODE FAILED" );
        }
    }

    printf( "\nfini\n" );
    return 0;
}

// --------------------------------------------------------------------------
// Unpack:  mfinspect --unpack <file.mflevel> <output_dir>
//
// Writes:
//   <output_dir>/<level_name>.mfdata       — raw FlatBuffer bytes
//   <output_dir>/<viewname>_<Type>.jxl     — one JXL per image entry
// --------------------------------------------------------------------------

static int cmd_unpack( const char* mflevel_path, const char* output_dir )
{
    mft::Level level;
    if( !mft::load_level( mflevel_path, level ) )
    {
        fprintf( stderr, "Failed to load %s\n", mflevel_path );
        return 1;
    }

    const std::filesystem::path out_dir( output_dir );
    std::error_code ec;
    std::filesystem::create_directories( out_dir, ec );
    if( ec )
    {
        fprintf( stderr, "Could not create output directory: %s\n", ec.message().c_str() );
        return 1;
    }

    const auto* fbs          = level.fbs();
    const int num_views      = static_cast<int>( fbs->views()->size() );
    const std::string lname  = fbs->name() ? fbs->name()->str() : "level";

    // Write FlatBuffer bytes as .mfdata
    const std::filesystem::path mfdata_path = out_dir / ( lname + ".mfdata" );
    {
        std::ofstream out( mfdata_path, std::ios::binary );
        if( !out )
        {
            fprintf( stderr, "Could not write %s\n", mfdata_path.string().c_str() );
            return 1;
        }
        out.write( level.flatbuffer_data.data(), static_cast<std::streamsize>( level.flatbuffer_data.size() ) );
    }
    printf( "Wrote %s\n", mfdata_path.string().c_str() );

    // Extract each JXL
    int written = 0;
    for( int i = 0; i < num_views; ++i )
    {
        const auto* view        = fbs->views()->Get( i );
        const std::string vname = view->name() ? view->name()->str() : "view" + std::to_string( i );

        for( int t = 0; t < NUM_IMAGE_TYPES; ++t )
        {
            const auto* entry = get_fbs_entry( view, IMAGE_TYPES[t] );
            if( !entry || entry->size() == 0 )
                continue;

            std::vector<char> compressed( static_cast<size_t>( entry->size() ) );
            if( !mft::read_image_data( level, i, IMAGE_TYPES[t], compressed.data(), compressed.size() ) )
            {
                fprintf( stderr, "  Failed to read %s/%s\n", vname.c_str(), IMAGE_TYPE_NAMES[t] );
                continue;
            }

            const std::filesystem::path jxl_path = out_dir / ( vname + "_" + IMAGE_TYPE_NAMES[t] + ".jxl" );
            {
                std::ofstream out( jxl_path, std::ios::binary );
                if( !out )
                {
                    fprintf( stderr, "  Could not write %s\n", jxl_path.string().c_str() );
                    continue;
                }
                out.write( compressed.data(), static_cast<std::streamsize>( compressed.size() ) );
            }
            printf( "  Wrote %s\n", jxl_path.filename().string().c_str() );
            ++written;
        }
    }

    printf( "Unpacked %d image(s) from %d view(s)\n", written, num_views );
    return 0;
}

// --------------------------------------------------------------------------
// Repack:  mfinspect --repack <input_dir> <output.mflevel>
//
// Reads:
//   <input_dir>/<any>.mfdata               — FlatBuffer bytes
//   <input_dir>/<viewname>_<Type>.jxl      — one JXL per image entry
//
// Patches the in-memory FlatBuffer with new blob offsets/sizes, then writes:
//   4-byte LE size prefix | patched FlatBuffer | new image blob
// --------------------------------------------------------------------------

static int cmd_repack( const char* input_dir, const char* output_mflevel )
{
    const std::filesystem::path in_dir( input_dir );

    // Find the .mfdata file
    std::filesystem::path mfdata_path;
    for( const auto& de : std::filesystem::directory_iterator( in_dir ) )
    {
        if( de.path().extension() == ".mfdata" )
        {
            mfdata_path = de.path();
            break;
        }
    }
    if( mfdata_path.empty() )
    {
        fprintf( stderr, "No .mfdata file found in %s\n", input_dir );
        return 1;
    }

    // Load FlatBuffer bytes
    std::vector<char> flatbuffer_data;
    {
        std::ifstream in( mfdata_path, std::ios::binary );
        if( !in )
        {
            fprintf( stderr, "Could not open %s\n", mfdata_path.string().c_str() );
            return 1;
        }
        flatbuffer_data.assign( std::istreambuf_iterator<char>( in ), {} );
    }

    if( flatbuffer_data.size() < 8 || std::memcmp( flatbuffer_data.data() + 4, mft::MFLEVEL_MAGIC, 4 ) != 0 )
    {
        fprintf( stderr, "Invalid FlatBuffer in %s (missing MFLV magic)\n", mfdata_path.string().c_str() );
        return 1;
    }

    const auto* fbs = data::GetLevel( flatbuffer_data.data() );
    if( !fbs || !fbs->views() )
    {
        fprintf( stderr, "Could not parse FlatBuffer from %s\n", mfdata_path.string().c_str() );
        return 1;
    }

    const int num_views = static_cast<int>( fbs->views()->size() );

    // Phase 1: read all JXL files into memory (fail fast on any missing file)
    struct ImageLoad
    {
        const data::ImageEntry* entry;
        std::vector<char>       data;
    };
    std::vector<ImageLoad> loads;
    bool any_error = false;

    for( int i = 0; i < num_views; ++i )
    {
        const auto* view        = fbs->views()->Get( i );
        const std::string vname = view->name() ? view->name()->str() : "view" + std::to_string( i );

        for( int t = 0; t < NUM_IMAGE_TYPES; ++t )
        {
            const auto* entry = get_fbs_entry( view, IMAGE_TYPES[t] );
            if( !entry || entry->size() == 0 )
                continue;

            const std::filesystem::path jxl_path = in_dir / ( vname + "_" + IMAGE_TYPE_NAMES[t] + ".jxl" );
            if( !std::filesystem::exists( jxl_path ) )
            {
                fprintf( stderr, "Missing: %s\n", jxl_path.string().c_str() );
                any_error = true;
                continue;
            }

            std::vector<char> jxl_data;
            {
                std::ifstream in( jxl_path, std::ios::binary );
                jxl_data.assign( std::istreambuf_iterator<char>( in ), {} );
            }
            loads.push_back( { entry, std::move( jxl_data ) } );
        }
    }

    if( any_error )
    {
        fprintf( stderr, "Aborting repack due to missing files\n" );
        return 1;
    }

    // Phase 2: build new blob, patch offsets/sizes in the FlatBuffer
    std::vector<char> new_blob;
    for( auto& img : loads )
    {
        const uint64_t new_offset = static_cast<uint64_t>( new_blob.size() );
        const uint64_t new_size   = static_cast<uint64_t>( img.data.size() );
        mutate_image_entry( img.entry, new_offset, new_size );
        new_blob.insert( new_blob.end(), img.data.begin(), img.data.end() );
    }

    // Phase 3: write .mflevel
    const uint32_t fb_size = static_cast<uint32_t>( flatbuffer_data.size() );
    std::ofstream out( output_mflevel, std::ios::binary );
    if( !out )
    {
        fprintf( stderr, "Could not write %s\n", output_mflevel );
        return 1;
    }
    out.write( reinterpret_cast<const char*>( &fb_size ), sizeof( fb_size ) );
    out.write( flatbuffer_data.data(), static_cast<std::streamsize>( flatbuffer_data.size() ) );
    out.write( new_blob.data(), static_cast<std::streamsize>( new_blob.size() ) );

    printf( "Repacked %d image(s) from %d view(s) — wrote %s\n",
            static_cast<int>( loads.size() ), num_views, output_mflevel );
    return 0;
}

// --------------------------------------------------------------------------
// Entry point
// --------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    if( argc >= 2 && std::strcmp( argv[1], "--unpack" ) == 0 )
    {
        if( argc != 4 )
        {
            fprintf( stderr, "Usage: mfinspect --unpack <file.mflevel> <output_dir>\n" );
            return 1;
        }
        return cmd_unpack( argv[2], argv[3] );
    }

    if( argc >= 2 && std::strcmp( argv[1], "--repack" ) == 0 )
    {
        if( argc != 4 )
        {
            fprintf( stderr, "Usage: mfinspect --repack <input_dir> <output.mflevel>\n" );
            return 1;
        }
        return cmd_repack( argv[2], argv[3] );
    }

    if( argc < 2 )
    {
        fprintf( stderr, "Usage:\n" );
        fprintf( stderr, "  mfinspect <file.mflevel>\n" );
        fprintf( stderr, "  mfinspect --unpack <file.mflevel> <output_dir>\n" );
        fprintf( stderr, "  mfinspect --repack <input_dir> <output.mflevel>\n" );
        return 1;
    }

    return cmd_inspect( argv[1] );
}
