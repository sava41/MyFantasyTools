#include "level_manager.h"

#include <stdio.h>

namespace mft {

bool LevelManager::loadLevel(const std::string& path) {
  FILE* file = fopen(path.c_str(), "r");
  if (!file) {
    fprintf(stderr, "Could not open %s\n", path.c_str());
    return false;
  }

  if (fclose(file) != 0) {
    fprintf(stderr, "Could not close %s\n", path.c_str());
    return false;
  }

  return true;
}

}  // namespace mft