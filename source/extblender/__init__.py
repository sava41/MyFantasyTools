import bpy
import sys
import os
import importlib

__external_path__ = os.path.join(os.path.dirname(__file__), "external")

if __external_path__ not in sys.path:
    sys.path.append(__external_path__)

from . ui import ui_main
from . import mftools

def register():
    importlib.reload(ui_main)
    ui_main.register()

def unregister():
    ui_main.unregister()

if __name__ == "__main__":
    register()