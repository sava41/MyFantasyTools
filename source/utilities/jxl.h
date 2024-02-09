#pragma once

#include <vector>

namespace mft {

bool EncodeJxlOneshot(const uint32_t xsize, const uint32_t ysize,
                      const uint32_t channels, const void* pixelData,
                      std::vector<char>& compressed);
}