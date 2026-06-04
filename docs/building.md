# Building & Development

## Requirements

- Blender 5.0
- Python 3.11
- CMake 3.19 or later
- C++ compiler
- Ninja (optional but recommended)

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=<type>
cmake --build build --target mflib mftools mfinspect mftest mft_godot
cmake --install build --component mft --prefix ./
```

If the build setup has issues finding Python, try manually setting `Python_ROOT_DIR` and `Python3_ROOT_DIR`.

## Plugin Development

### Blender Plugin

For Blender plugin development, [this VSCode extension](https://marketplace.visualstudio.com/items?itemName=JacquesLucke.blender-development) makes iteration fast. Set the plugin path to the install directory to enable hot reload as you code.

### Godot Plugin

Not sure on best practices. After building, I just install the plugin directly into my Godot project:
`cmake --install build --component mft --prefix <godot project path>/addons/`


