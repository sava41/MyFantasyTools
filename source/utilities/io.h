#pragma once

#include <filesystem>
#include <vector>

namespace mft {

bool write_binary(const std::filesystem::path& binPath,
                  const std::vector<char>& buffer);

std::vector<char> read_binary(const std::filesystem::path& binPath);

}  // namespace mft