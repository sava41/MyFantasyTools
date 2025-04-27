import bpy
import sys
import os
import importlib

from . ui import ui_main
from . core import data_models
from . export import export_main
from . import mftools

def register():
    importlib.reload(data_models)
    importlib.reload(ui_main)
    importlib.reload(export_main)

    data_models.register()
    export_main.register()
    ui_main.register()

def unregister():
    ui_main.unregister()
    export_main.unregister()
    data_models.unregister()

if __name__ == "__main__":
    register()