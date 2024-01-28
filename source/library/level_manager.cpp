#include "level_manager.h"

#include <fstream>

#include "level_generated.h"

namespace mft {

bool LevelManager::loadLevel(const std::string& path) {
  std::ifstream levelFile;
  levelFile.open(path, std::ios::binary | std::ios::in);

  if (!levelFile.good()) {
    fprintf(stderr, "Could not open %s\n", path.c_str());
    return false;
  }

  levelFile.seekg(0, std::ios::end);
  int length = levelFile.tellg();
  levelFile.seekg(0, std::ios::beg);
  std::unique_ptr<char[]> data = std::make_unique<char[]>(length);
  levelFile.read(data.get(), length);

  auto level = MFT::GetLevel(data.get());

  levelFile.close();

  return true;
}

}  // namespace mft