#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "io.h"

namespace fs = std::filesystem;

class IoTest : public ::testing::Test
{
protected:
    fs::path tmp_dir;

    void SetUp() override
    {
        tmp_dir = fs::temp_directory_path() / "mftest_io";
        fs::create_directories( tmp_dir );
    }

    void TearDown() override
    {
        fs::remove_all( tmp_dir );
    }
};

// ---- write_binary / read_binary --------------------------------------------

TEST_F( IoTest, RoundTrip )
{
    std::vector<char> data = { 'H', 'e', 'l', 'l', 'o' };
    auto path              = tmp_dir / "test.bin";

    ASSERT_TRUE( mft::write_binary( path, data ) );
    EXPECT_EQ( mft::read_binary( path ), data );
}

TEST_F( IoTest, WriteEmptyFile )
{
    auto path = tmp_dir / "empty.bin";
    ASSERT_TRUE( mft::write_binary( path, {} ) );
    EXPECT_EQ( fs::file_size( path ), 0u );
    EXPECT_TRUE( mft::read_binary( path ).empty() );
}

TEST_F( IoTest, PreservesAllByteValues )
{
    // One byte per value 0–255 to catch any sign / text-mode surprises
    std::vector<char> data( 256 );
    for( int i = 0; i < 256; ++i )
        data[i] = static_cast<char>( i );

    auto path = tmp_dir / "allbytes.bin";
    ASSERT_TRUE( mft::write_binary( path, data ) );
    EXPECT_EQ( mft::read_binary( path ), data );
}

TEST_F( IoTest, ReadNonexistentReturnsEmpty )
{
    EXPECT_TRUE( mft::read_binary( tmp_dir / "does_not_exist.bin" ).empty() );
}

// ---- read_mflevel ----------------------------------------------------------

TEST_F( IoTest, ReadMflevelNonexistentReturnsFalse )
{
    std::vector<char> fb;
    size_t            blob{};
    EXPECT_FALSE( mft::read_mflevel( tmp_dir / "noexist.mflevel", fb, blob ) );
}

TEST_F( IoTest, ReadMflevelTruncatedSizePrefixReturnsFalse )
{
    auto path = tmp_dir / "truncated.mflevel";
    {
        std::ofstream f( path, std::ios::binary );
        f.write( "\x04\x00", 2 ); // only 2 bytes — size prefix needs 4
    }
    std::vector<char> fb;
    size_t            blob{};
    EXPECT_FALSE( mft::read_mflevel( path, fb, blob ) );
}

TEST_F( IoTest, ReadMflevelWrongMagicReturnsFalse )
{
    auto path = tmp_dir / "badmagic.mflevel";
    {
        std::ofstream f( path, std::ios::binary );
        uint32_t fb_size = 8;
        f.write( reinterpret_cast<char*>( &fb_size ), 4 );
        f.write( "\x04\x00\x00\x00", 4 ); // root offset
        f.write( "WXYZ", 4 );              // wrong file identifier
    }
    std::vector<char> fb;
    size_t            blob{};
    EXPECT_FALSE( mft::read_mflevel( path, fb, blob ) );
}

TEST_F( IoTest, ReadMflevelTooShortForFlatbufferSectionReturnsFalse )
{
    // Claims 100 bytes of flatbuffer data, but file ends after the size prefix
    auto path = tmp_dir / "short_fb.mflevel";
    {
        std::ofstream f( path, std::ios::binary );
        uint32_t fb_size = 100;
        f.write( reinterpret_cast<char*>( &fb_size ), 4 );
        // No actual data follows
    }
    std::vector<char> fb;
    size_t            blob{};
    EXPECT_FALSE( mft::read_mflevel( path, fb, blob ) );
}

TEST_F( IoTest, ReadMflevelValidHeader )
{
    auto path = tmp_dir / "valid.mflevel";
    {
        std::ofstream f( path, std::ios::binary );
        uint32_t fb_size = 8;
        f.write( reinterpret_cast<char*>( &fb_size ), 4 );
        f.write( "\x04\x00\x00\x00", 4 ); // root table offset
        f.write( "MFLV", 4 );              // correct file identifier
    }

    std::vector<char> fb;
    size_t            blob{};
    ASSERT_TRUE( mft::read_mflevel( path, fb, blob ) );

    EXPECT_EQ( fb.size(), 8u );
    EXPECT_EQ( blob, sizeof( uint32_t ) + 8u );
    EXPECT_EQ( std::memcmp( fb.data() + 4, "MFLV", 4 ), 0 );
}
