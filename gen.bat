set RESOLUTION=100
set SAMPLINGS=1024

# Run the scripts
blender --background ./level.blend --python ./tools/process_level.py -- ./output/ %RESOLUTION% %SAMPLINGS%