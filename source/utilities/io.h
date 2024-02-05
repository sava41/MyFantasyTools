#pragma once

#include <filesystem>
#include <vector>

namespace mft {

bool WriteBinary(const std::filesystem::path& binPath,
                 const std::vector<char>& buffer);

std::vector<char> ReadBinary(const std::filesystem::path& binPath);

}  // namespace mft