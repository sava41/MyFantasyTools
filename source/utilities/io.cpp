#include "io.h"

#include <fstream>
#include <iostream>

namespace mft {

bool WriteBinary(const std::filesystem::path& binPath,
                 const std::vector<char>& buffer) {
  std::ofstream binary(binPath, std::ios::binary);

  binary.write(buffer.data(), buffer.size());

  bool res = true;
  if (!binary.good()) {
    fprintf(stderr, "Could not write %s\n", binPath.string().c_str());
    res = false;
  }

  binary.close();

  return res;
}

std::vector<char> ReadBinary(const std::filesystem::path& binPath) {
  std::vector<char> buffer;

  std::ifstream binary;
  binary.open(binPath, std::ios::binary | std::ios::in);

  if (!binary.good()) {
    fprintf(stderr, "Could not open %s\n", binPath.string().c_str());
  } else {
    buffer.reserve(std::filesystem::file_size(binPath));
    buffer.assign(std::istreambuf_iterator<char>(binary), {});
  }

  binary.close();

  return std::move(buffer);
}
}  // namespace mft