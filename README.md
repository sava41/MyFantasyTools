# My Fantasy Tools

<p align="center">
	<img src="docs/wizard.svg" height="200" alt="My Fantasy Tools logo">
</p>

A pipeline for making pre-rendered background, fixed-perspective camera games like PS1 Resident Evil and Final Fantasy. End-to-end tooling from background creation in Blender to level loading in your game engine.

![Example Video](docs/demo.avif)

## Features

- Blender plugin for defining cameras, navmesh, shadow lights, and render settings in one panel
- Camera rotation bounds system — author the allowed pan/tilt range per view and automatically determine the required background render size
- Background rendering pipeline that bakes depth and lighting data alongside the beauty image
- `.mflevel` file format — a single file containing all scene data for a level
- Godot 4 plugin with an `MFLevel` node for importing and playing back levels
- C++ library for reading `.mflevel` files in any engine

## Getting Started

- [Tutorial](docs/tutorial.md) — step-by-step guide from Blender scene setup to loading a level in Godot
- [Building & Development](docs/building.md) — requirements, build instructions, and plugin development tips

## License

Released under the [MIT License](LICENSE).
