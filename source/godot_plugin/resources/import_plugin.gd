# import_plugin.gd
@tool
extends EditorImportPlugin

func _get_importer_name():
	return "mflevel.plugin"

func _get_visible_name():
	return "My Fantasy Level"

func _get_recognized_extensions():
	return ["mflevel"]

func _get_save_extension():
	return "mflevel"

func _get_preset_count():
	return 1

func  _get_import_order():
	return 0
	
func _get_priority():
	return 1.0

func _get_preset_name(preset_index):
	return "Default"
	
func _get_resource_type():
	return ""
	
func _get_import_options(path, preset_index):
	return []

func _import(source_file, save_path, options, r_platform_variants, r_gen_files):
	DirAccess.copy_absolute(source_file, save_path + "." + _get_save_extension())

	var current_scene_root = EditorInterface.get_edited_scene_root()

	if !current_scene_root:
		return ERR_CANT_CREATE

	var level = MFLevel.new()
	level.name = source_file.get_file().get_basename()
	level.set_level_file_path(source_file)
	current_scene_root.add_child(level)
	level.owner = current_scene_root

	return OK
