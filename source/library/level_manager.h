

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace mft {

class LevelManager {
 public:
  LevelManager() = default;
  ~LevelManager() = default;

  bool load_level(const std::string& path);
  int get_views_length();

 private:
  std::vector<char> m_data_buffer;
  //  std::vector<ViewData> m_views;
};
}  // namespace mft
