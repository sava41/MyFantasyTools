#include "level_manager.h"

#include <filesystem>
#include <fstream>

#include "io.h"
#include "jxl.h"
#include "level_generated.h"

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

  m_data_buffer = read_binary(levelFilePath);

  const auto* level =
      flattbuffer::GetLevel(reinterpret_cast<void*>(m_data_buffer.data()));

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

}  // namespace mft