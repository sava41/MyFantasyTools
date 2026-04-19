#include "io.h"

#include <cstring>
#include <fstream>
#include <iostream>

namespace mft
{

    bool write_binary( const std::filesystem::path& binPath, const std::vector<char>& buffer )
    {
        std::ofstream binary( binPath, std::ios::binary );

        bool res = true;
        if( !binary.good() )
        {
            fprintf( stderr, "Could not write %s\n", binPath.string().c_str() );
            res = false;
        }
        else
        {
            binary.write( buffer.data(), buffer.size() );
        }

        binary.close();

        return res;
    }

    std::vector<char> read_binary( const std::filesystem::path& binPath )
    {
        std::vector<char> buffer;

        std::ifstream binary;
        binary.open( binPath, std::ios::binary | std::ios::in );

        if( !binary.good() )
        {
            fprintf( stderr, "Could not open %s\n", binPath.string().c_str() );
        }
        else
        {
            buffer.reserve( std::filesystem::file_size( binPath ) );
            buffer.assign( std::istreambuf_iterator<char>( binary ), {} );
        }

        binary.close();

        return std::move( buffer );
    }

    bool read_mflevel( const std::filesystem::path& path,
                       std::vector<char>&           flatbuffer_out,
                       std::vector<char>&           image_blob_out )
    {
        std::ifstream file( path, std::ios::binary | std::ios::in );
        if( !file.good() )
        {
            fprintf( stderr, "Could not open %s\n", path.string().c_str() );
            return false;
        }

        // Read 4-byte LE size prefix.
        uint32_t fb_size = 0;
        file.read( reinterpret_cast<char*>( &fb_size ), sizeof( fb_size ) );
        if( !file.good() )
        {
            fprintf( stderr, "Failed to read size prefix from %s\n", path.string().c_str() );
            return false;
        }

        flatbuffer_out.resize( fb_size );
        file.read( flatbuffer_out.data(), fb_size );
        if( !file.good() )
        {
            fprintf( stderr, "Failed to read FlatBuffer section from %s\n", path.string().c_str() );
            return false;
        }

        // Validate "MFLV" file identifier at bytes [4..7] of the FlatBuffer.
        if( fb_size < 8 || std::memcmp( flatbuffer_out.data() + 4, MFLEVEL_MAGIC, 4 ) != 0 )
        {
            fprintf( stderr, "Invalid file identifier in %s\n", path.string().c_str() );
            return false;
        }

        image_blob_out.assign( std::istreambuf_iterator<char>( file ), {} );

        return true;
    }

} // namespace mft