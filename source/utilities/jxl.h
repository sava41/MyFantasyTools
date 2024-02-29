#pragma once

#include <vector>

namespace mft::jxl {

bool encode_oneshot(const uint32_t xsize, const uint32_t ysize,
                    const uint32_t channels, const void* pixelData,
                    std::vector<char>& compressed);

bool decode_oneshot(std::vector<char>& compressed, std::vector<float>& pixels,
                    size_t& xsize, size_t& ysize,
                    std::vector<uint8_t>& iccProfile);

}  // namespace mft::jxl