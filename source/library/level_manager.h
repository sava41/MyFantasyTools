#include <string>

#pragma once

namespace mft {
class LevelManager {
 public:
  LevelManager() = default;
  ~LevelManager() = default;

  bool loadLevel(const std::string& path);
};
}  // namespace mft
