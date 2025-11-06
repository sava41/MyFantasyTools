<p align="center">
	<img src="docs/wizard.svg" height="256" alt="My Fantasy Tools logo">
	<br><em><b>Pre-rendered background game tools</b></em></br>
</p>

This project is a pipeline for making pre-rendered background games in the style of PS1 Resident Evil and Final Fantasy. The tools are intended as a end-to-end solution from the creation of the backgrounds in Blender to the loading of the data in your game engine of choice.

## Requiremends
- Blender 4.0 or later
- Python 3.11
- CMake 3.19 or later
- C++ compiler
- Ninja (optional but recomended)

## Building
Build project:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=<type>
cmake --build build --target mflib mftools mft_godot
cmake --install build --component mft
```

Note: If the build setup has issues finding python, try manually setting `Python_ROOT_DIR` and `Python3_ROOT_DIR`

## Usage

### Blender Plugin
The Blender plugin automates exporting a Blender scene into an MFT level. After building and installing the project, the `mft_blender_addon` directory can be zipped and installed using the `Install from disk` option in Blenders settings. Pre-built addons can be found in the releases (Windows and MacOs only).

For plugin development I like to use https://marketplace.visualstudio.com/items?itemName=JacquesLucke.blender-development. You can set the plugin path to the install directory and hot reload as you code. 

### Godot Plugin
The Godot addon allows you to import and use `.mflevel` files. After building and installing the plugin will be found in the `mft_godot_plugin` directory. 

### C++ Library
**tbd:** The libary has an api to read MFT level files and stream level data for use with a game engine.