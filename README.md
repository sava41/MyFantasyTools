<p align="center">
	<img src="docs/wizard.svg" height="200" alt="My Fantasy Tools logo">
</p>

# My Fantasy Tools

A pipeline for making pre-rendered background, fixed-perspective camera games like PS1 Resident Evil and Final Fantasy. End-to-end tooling from background scene creation to level loading in a game engine.

![Example Video](docs/demo.avif)

## Features

- Blender plugin for defining cameras, navmesh, shadow lights, and render settings
- Camera rotation bounds system: author the allowed pan/tilt range per view and automatically determine the required background render size
- Background rendering pipeline that bakes all necessary data and outputs a single `.mflevel` file containing all data for a level
- Godot plugin with an `MFLevel` node 3d for adding pre-rendered backgrounds to your scene tree
- C++ library for reading `.mflevel` files in any engine

## Getting Started

- [User Guide](docs/tutorial.md) — step-by-step guide from Blender scene setup to loading a level in Godot
- [Building & Development](docs/building.md) — build instructions, and plugin development tips

## License

Released under the [MIT License](LICENSE).
