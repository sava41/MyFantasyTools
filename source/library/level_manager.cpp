#include "level_manager.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include "io.h"
#include "jxl.h"
#include "level_generated.h"

namespace mft {

class ViewData {
 public:
  ViewData(const std::string& name, int sizex, int sizey)
      : m_name(name),
        m_sizex(sizex),
        m_sizey(sizey),
        m_aspectRatio(static_cast<float>(sizex) / static_cast<float>(sizey)) {
    m_colorBuffer.reserve(m_sizex * m_sizey * sizeof(float));
    m_depthBuffer.reserve(m_sizex * m_sizey * sizeof(float));
  }
  ~ViewData() = default;

  ViewData(const ViewData&) = delete;
  ViewData& operator=(const ViewData&) = delete;

  ViewData(ViewData&&) = default;
  ViewData& operator=(ViewData&&) = default;

  void loadColorBuffer(const std::filesystem::path& filePath) {
    size_t fileSizex, fileSizey;
    std::vector<uint8_t> iccProfile;

    std::vector<char> compressed = ReadBinary(filePath);
    bool ret = jxl::DecodeOneShot(compressed, m_colorBuffer, fileSizex,
                                  fileSizey, iccProfile);

    if (fileSizex != m_sizex || fileSizey != m_sizey) m_colorBuffer.clear();
  };
  void loadDepthBuffer(const std::filesystem::path& filePath) {
    size_t fileSizex, fileSizey;
    std::vector<uint8_t> iccProfile;

    std::vector<char> compressed = ReadBinary(filePath);
    bool ret = jxl::DecodeOneShot(compressed, m_depthBuffer, fileSizex,
                                  fileSizey, iccProfile);

    if (fileSizex != m_sizex || fileSizey != m_sizey) m_depthBuffer.clear();
  };

 private:
  std::string m_name;

  int m_sizex;
  int m_sizey;
  float m_aspectRatio;

  std::vector<int> m_adjacentViews;

  std::vector<float> m_colorBuffer;
  std::vector<float> m_depthBuffer;
};

bool LevelManager::loadLevel(const std::string& pathString) {
  const std::filesystem::path levelFilePath(pathString);

  std::ifstream levelFile;
  levelFile.open(levelFilePath, std::ios::binary | std::ios::in);

  if (!levelFile.good()) {
    fprintf(stderr, "Could not open %s\n", levelFilePath.string().c_str());
    return false;
  }

  levelFile.seekg(0, std::ios::end);
  const int length = levelFile.tellg();
  levelFile.seekg(0, std::ios::beg);
  std::unique_ptr<char[]> data = std::make_unique<char[]>(length);
  levelFile.read(data.get(), length);

  const auto level = MFT::GetLevel(data.get());

  printf("Data path: %s\n", level->data_path()->c_str());

  const auto viewsData = level->views();

  std::vector<ViewData> views;
  views.reserve(viewsData->size());

  for (int i = 0; i < viewsData->size(); ++i) {
    const auto viewData = viewsData->Get(i);
    const std::filesystem::path colorDataFilePath =
        levelFilePath.parent_path() /
        std::filesystem::path(level->data_path()->str() +
                              viewData->name()->str() + "_Color.jxl");

    const std::filesystem::path depthDataFilePath =
        levelFilePath.parent_path() /
        std::filesystem::path(level->data_path()->str() +
                              viewData->name()->str() + "_Depth.jxl");

    views.push_back(
        {viewData->name()->str(), viewData->resX(), viewData->resY()});
    views.back().loadColorBuffer(colorDataFilePath);
    views.back().loadDepthBuffer(depthDataFilePath);

    printf("loaded %s\n", viewData->name()->c_str());
  }

  levelFile.close();

  return true;
}

}  // namespace mft