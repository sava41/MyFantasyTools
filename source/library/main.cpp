#include <stdio.h>

#include "level_generated.h"
#include "level_manager.h"

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Enter path to level");
    return 1;
  }

  mft::LevelManager manager;
  if (!manager.loadLevel(argv[1])) {
    return 1;
  }

  printf("fini");
  return 0;
}