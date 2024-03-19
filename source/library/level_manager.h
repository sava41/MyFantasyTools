

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

constexpr int MAT4_SIZE = 16;

namespace mft {

class ViewData;

class LevelManager {
 public:
  LevelManager();
  ~LevelManager();

  bool load_level(const std::string& path);
  int get_views_length() const;

  std::string get_view_name(int viewIndex) const;
  int get_view_width(int viewIndex) const;
  int get_view_height(int viewIndex) const;
  std::array<float, MAT4_SIZE> get_view_tranform(int viewIndex) const;
  float get_view_fov(int viewIndex) const;

  float* get_view_color_buffer(int viewIndex) const;
  float* get_view_depth_buffer(int viewIndex) const;
  float* get_view_env_buffer(int viewIndex) const;

  std::vector<int> get_adjacent_views(int viewIndex) const;

  int get_view_id_from_position(float x, float y, float z) const;

  int get_navmesh_tris_length();
  std::array<float, 9> get_navmesh_tri_verts(int triIndex);

 private:
  std::vector<char> m_dataBuffer;
  std::vector<ViewData> m_views;
};

}  // namespace mft
