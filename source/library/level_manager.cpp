#include "level_manager.h"

#include <filesystem>
#include <fstream>
#include <limits>

#include "io.h"
#include "jxl.h"
#include "level_generated.h"
#include "math.h"

namespace mft {

class ViewData {
 public:
  ViewData(const std::string& name, int sizex, int sizey,
           const std::filesystem::path& dataDir)
      : m_name(name),
        m_sizex(sizex),
        m_sizey(sizey),
        m_aspectRatio(static_cast<float>(sizex) / static_cast<float>(sizey)),
        m_colorBinPath(dataDir / std::filesystem::path(name + "_Color.jxl")),
        m_depthBinPath(dataDir / std::filesystem::path(name + "_Depth.jxl")),
        m_loaded(false){};
  ~ViewData() = default;

  ViewData(const ViewData&) = delete;
  ViewData& operator=(const ViewData&) = delete;
  ViewData& operator=(ViewData&&) = delete;

  ViewData(ViewData&& other) noexcept
      : m_name(std::move(other.m_name)),
        m_sizex(other.m_sizex),
        m_sizey(other.m_sizey),
        m_aspectRatio(other.m_aspectRatio),
        m_colorBinPath(std::move(other.m_colorBinPath)),
        m_depthBinPath(std::move(other.m_depthBinPath)),
        m_loaded(other.m_loaded.load()),
        m_colorBuffer(std::move(other.m_colorBuffer)),
        m_depthBuffer(std::move(other.m_depthBuffer)) {
    other.m_loaded.store(false);
    m_sizex = 0;
    m_sizey = 0;
    m_aspectRatio = 0.0;
  };

  bool load_data() {
    size_t fileSizex, fileSizey;
    std::vector<uint8_t> iccProfile;

    {
      m_colorBuffer.reserve(m_sizex * m_sizey * sizeof(float));
      std::vector<char> compressed = read_binary(m_colorBinPath);
      bool res = jxl::decode_oneshot(compressed, m_colorBuffer, fileSizex,
                                     fileSizey, iccProfile);
      if (fileSizex != m_sizex || fileSizey != m_sizey || !res) {
        unload_data();
        return false;
      }
    }

    {
      m_depthBuffer.reserve(m_sizex * m_sizey * sizeof(float));
      std::vector<char> compressed = read_binary(m_depthBinPath);
      bool res = jxl::decode_oneshot(compressed, m_depthBuffer, fileSizex,
                                     fileSizey, iccProfile);
      if (fileSizex != m_sizex || fileSizey != m_sizey || !res) {
        unload_data();
        return false;
      }
    }

    m_loaded = true;

    return true;
  };

  void unload_data() {
    m_depthBuffer.clear();
    m_colorBuffer.clear();

    m_loaded = false;
  }

 private:
  std::string m_name;

  int m_sizex;
  int m_sizey;
  float m_aspectRatio;

  std::filesystem::path m_colorBinPath;
  std::filesystem::path m_depthBinPath;

  std::vector<float> m_colorBuffer;
  std::vector<float> m_depthBuffer;

  std::atomic<bool> m_loaded;
};

bool LevelManager::load_level(const std::string& pathString) {
  const std::filesystem::path levelFilePath(pathString);

  m_dataBuffer = read_binary(levelFilePath);

  const auto* level =
      data::GetLevel(reinterpret_cast<void*>(m_dataBuffer.data()));

  const std::filesystem::path dataFilePath =
      levelFilePath.parent_path() /
      std::filesystem::path(level->data_path()->str());

  printf("Data path: %s\n", level->data_path()->c_str());

  const auto* viewsData = level->views();

  std::vector<ViewData> views;
  views.reserve(viewsData->size());

  for (int i = 0; i < viewsData->size(); ++i) {
    const auto* const viewData = viewsData->Get(i);

    views.emplace_back(viewData->name()->str(), viewData->res_x(),
                       viewData->res_y(), dataFilePath);
    views.back().load_data();

    printf("loaded %s\n", viewData->name()->c_str());
  }

  return true;
}

int LevelManager::get_views_length() {
  const auto* level =
      data::GetLevel(reinterpret_cast<void*>(m_dataBuffer.data()));

  return level->views()->size();
}

std::array<float, MAT4_SIZE> LevelManager::get_view_tranform(int viewIndex) {
  const auto* level =
      data::GetLevel(reinterpret_cast<void*>(m_dataBuffer.data()));

  if (0 > viewIndex || viewIndex >= level->views()->size()) return {};

  const auto* worldTransform =
      level->views()->Get(viewIndex)->world_transform();

  return {worldTransform->m00(), worldTransform->m01(), worldTransform->m02(),
          worldTransform->m03(), worldTransform->m10(), worldTransform->m11(),
          worldTransform->m12(), worldTransform->m13(), worldTransform->m20(),
          worldTransform->m21(), worldTransform->m22(), worldTransform->m23(),
          worldTransform->m30(), worldTransform->m31(), worldTransform->m32(),
          worldTransform->m33()};
}

std::vector<int> LevelManager::get_adjacent_views(int viewIndex) {
  const auto* level =
      data::GetLevel(reinterpret_cast<void*>(m_dataBuffer.data()));

  if (0 > viewIndex || viewIndex >= level->views()->size()) return {};

  const auto* adjacentViews = level->views()->Get(viewIndex)->adjacent_views();

  return std::vector<int>(adjacentViews->begin(), adjacentViews->end());
}

int LevelManager::get_view_id_from_position(float x, float y, float z) {
  const auto* level =
      data::GetLevel(reinterpret_cast<void*>(m_dataBuffer.data()));

  const auto* verts = level->navmesh_verts();

  data::Vec3 point(x, y, z);

  float minDistance = std::numeric_limits<float>::max();
  int id = 0;

  for (auto triangle = level->navmesh_tris()->begin();
       triangle != level->navmesh_tris()->end(); ++triangle) {
    data::Vec3 closestPoint = closesPointOnTriangle(
        *verts->Get(triangle->vert1()), *verts->Get(triangle->vert2()),
        *verts->Get(triangle->vert3()), point);
    float distance = length(sub(point, closestPoint));

    if (distance < minDistance) {
      minDistance = distance;
      id = triangle->view_id();
    }
  }

  return id;
}

}  // namespace mft