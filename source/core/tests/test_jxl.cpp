#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "jxl.h"

// ---- encode ----------------------------------------------------------------

TEST( JxlEncode, SingleChannelSucceeds )
{
    constexpr uint32_t W = 4, H = 4;
    std::vector<float> pixels( W * H, 0.5f );
    std::vector<char>  compressed;
    ASSERT_TRUE( mft::jxl::encode_oneshot( W, H, 1, pixels.data(), compressed ) );
    EXPECT_FALSE( compressed.empty() );
}

TEST( JxlEncode, ThreeChannelSucceeds )
{
    constexpr uint32_t W = 4, H = 4;
    std::vector<float> pixels( W * H * 3, 0.5f );
    std::vector<char>  compressed;
    ASSERT_TRUE( mft::jxl::encode_oneshot( W, H, 3, pixels.data(), compressed ) );
    EXPECT_FALSE( compressed.empty() );
}

TEST( JxlEncode, UniformImageCompressesWell )
{
    // A constant-colour image should compress dramatically
    constexpr uint32_t W = 64, H = 64;
    std::vector<float> pixels( W * H, 0.5f );
    std::vector<char>  compressed;
    ASSERT_TRUE( mft::jxl::encode_oneshot( W, H, 1, pixels.data(), compressed ) );
    EXPECT_LT( compressed.size(), W * H * sizeof( float ) );
}

// ---- decode ----------------------------------------------------------------

TEST( JxlDecode, InvalidDataReturnsFalse )
{
    std::vector<char>  garbage = { 'J', 'U', 'N', 'K', 0, 0, 0, 0 };
    std::vector<float> out( 16 );
    EXPECT_FALSE( mft::jxl::decode_jxl( garbage.data(), garbage.size(),
                                        out.data(), out.size() * sizeof( float ) ) );
}

// ---- round-trip ------------------------------------------------------------

TEST( JxlRoundTrip, SingleChannel )
{
    constexpr uint32_t W = 8, H = 8;
    std::vector<float> original( W * H, 0.75f );

    std::vector<char> compressed;
    ASSERT_TRUE( mft::jxl::encode_oneshot( W, H, 1, original.data(), compressed ) );

    std::vector<float> decoded( W * H );
    ASSERT_TRUE( mft::jxl::decode_jxl( compressed.data(), compressed.size(),
                                        decoded.data(), decoded.size() * sizeof( float ) ) );

    for( size_t i = 0; i < decoded.size(); ++i )
        EXPECT_NEAR( decoded[i], original[i], 1e-2f ) << "mismatch at pixel " << i;
}

TEST( JxlRoundTrip, ThreeChannel )
{
    constexpr uint32_t W = 4, H = 4;
    // Alternating red pixels: (1, 0, 0) repeated
    std::vector<float> original( W * H * 3 );
    for( size_t i = 0; i < original.size(); ++i )
        original[i] = ( i % 3 == 0 ) ? 1.0f : 0.0f;

    std::vector<char> compressed;
    ASSERT_TRUE( mft::jxl::encode_oneshot( W, H, 3, original.data(), compressed ) );

    std::vector<float> decoded( W * H * 3 );
    ASSERT_TRUE( mft::jxl::decode_jxl( compressed.data(), compressed.size(),
                                        decoded.data(), decoded.size() * sizeof( float ) ) );

    for( size_t i = 0; i < decoded.size(); ++i )
        EXPECT_NEAR( decoded[i], original[i], 1e-2f ) << "mismatch at component " << i;
}
