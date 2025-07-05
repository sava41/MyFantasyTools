#pragma once

#include <functional>
#include <vector>

namespace mft::jxl
{

    typedef std::function<void*( int, int, int, size_t )> buffer_allocator;

    bool encode_oneshot( const uint32_t xsize, const uint32_t ysize, const uint32_t channels, const void* pixelData, std::vector<char>& compressed );

    bool decode_oneshot( std::vector<char>& compressed, const buffer_allocator& allocator );

} // namespace mft::jxl