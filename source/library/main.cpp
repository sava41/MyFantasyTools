#include <stdio.h>

#include "level_manager.h"

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Enter path to level");
    return 1;
  }

  mft::LevelManager manager;
  if (!manager.load_level(argv[1])) {
    return 1;
  }

  printf("there are %d cameras in this scene\n", manager.get_views_length());
  printf("there are %d triangles in the navmesh\n",
         manager.get_navmesh_tris_length());

  printf("fini");
  return 0;
}