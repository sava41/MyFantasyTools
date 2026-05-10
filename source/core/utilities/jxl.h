#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mft::jxl
{

    bool encode_oneshot( uint32_t xsize, uint32_t ysize, uint32_t channels, const void* pixels, std::vector<char>& compressed );

    // Decode a JXL image into a caller-provided buffer.
    // out_pixels must point to at least out_pixel_bytes bytes of writable memory.
    // Size should be: width * height * channels * sizeof(float)
    // where width/height/channels can be read from the Level flatbuffer (View::res_x/res_y).
    bool decode_jxl( const void* compressed, size_t compressed_size, void* out_pixels, size_t out_pixel_bytes );

} // namespace mft::jxl
