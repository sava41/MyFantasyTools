#pragma once

#include <vector>

namespace mft::jxl {

bool EncodeOneshot(const uint32_t xsize, const uint32_t ysize,
                   const uint32_t channels, const void* pixelData,
                   std::vector<char>& compressed);

bool DecodeOneShot(std::vector<char>& compressed, std::vector<float>& pixels,
                   size_t& xsize, size_t& ysize,
                   std::vector<uint8_t>& iccProfile);

}  // namespace mft::jxl