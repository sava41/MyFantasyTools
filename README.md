<p align="center">
	<img src="docs/wizard.svg" height="256" alt="My Fantasy Tools logo">
	<br><em><b>Pre-rendered background game tools</b></em></br>
</p>

This project is a pipeline for making pre-rendered background games in the style of ps1 resident evil and final fantasy. The tools are intended as a end-to-end solution from the creation of the backgrounds in Blender to the loading of the data in your game engine of choice.

## Requiremends
- Python 3.10
- CMake 3.19 or later
- gcc or msvc (clang not tested)

## Building
Install python dependancies:
```bash
> pip install -r requirements.txt
```

### Windows
Build project:
```bash
> mkdir build 
> cd build 
> cmake ..
> cmake --build . --target mflib mftools --config <Mode>
```

### Linux
**tbd**

## Usage

### Blender Plugin
**tbd:** The Blender plugin streamlines the creation of MFT compatible Blender files.

### Generator
A Blender file with the correct data (such as the example `level.blend`) can be generated into an MFT level with the following command:
```bash
> .\gen.bat .\level.blend
```

### C++ Library
**tbd:** The libary has an api to read MFT level files and stream level data for use with a game engine.