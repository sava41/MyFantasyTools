import bpy
import sys
import os

__addon_dir__ = os.path.dirname(os.path.abspath(__file__))

if __addon_dir__ not in sys.path:
    sys.path.append(__addon_dir__)

__external_path__ = os.path.join(os.path.dirname(__file__), "external")

if __external_path__ not in sys.path:
    sys.path.append(__external_path__)

from . ui import ui_main

bl_info = {
    "name": "My Fantasy Tools",
    "author": "Sava",
    "version": (1, 0),
    "blender": (4, 0, 0),
    "location": "View3D > UI > My Fantasy Tools",
    "description": "Make pre-rendered background games",
    "category": "3D View",
}

def register():
    ui_main.register()

def unregister():
    ui_main.unregister()

if __name__ == "__main__":
    register()