import bpy
import sys
import os
import importlib

from . ui import ui_main
from . core import data_models
from . import mftools

def register():
    importlib.reload(data_models)
    importlib.reload(ui_main)

    data_models.register()
    ui_main.register()

def unregister():
    data_models.unregister()
    ui_main.unregister()

if __name__ == "__main__":
    register()