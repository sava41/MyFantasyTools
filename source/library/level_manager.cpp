#include "level_manager.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include "level_generated.h"

namespace mft {

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

  const auto views = level->views();

  for (int i = 0; i < views->size(); ++i) {
    const auto view = views->Get(i);
    const std::filesystem::path dataFilePath =
        levelFilePath.parent_path() /
        std::filesystem::path(level->data_path()->str() + view->name()->str() +
                              "_Color.jxl");

    std::ifstream dataFile;
    dataFile.open(dataFilePath, std::ios::binary | std::ios::in);

    if (!dataFile.good()) {
      fprintf(stderr, "Could not open %s\n", dataFilePath.string().c_str());
    }

    dataFile.close();
  }

  levelFile.close();

  return true;
}

}  // namespace mft