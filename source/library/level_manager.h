

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

constexpr int MAT4_SIZE = 16;

namespace mft {

class LevelManager {
 public:
  LevelManager() = default;
  ~LevelManager() = default;

  bool load_level(const std::string& path);
  int get_views_length();
  std::array<float, MAT4_SIZE> get_view_tranform(int viewIndex);
  std::vector<int> get_adjacent_views(int viewIndex);
  int get_view_id_from_position(float x, float y, float z);

 private:
  std::vector<char> m_dataBuffer;
  //  std::vector<ViewData> m_views;
};

}  // namespace mft
