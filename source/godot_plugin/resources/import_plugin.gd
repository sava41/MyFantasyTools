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
	var reader := ZIPReader.new()
	var err := reader.open(source_file)
	if err != OK:
		return FileAccess.get_open_error()
		
	var level_file_path = ""
	var level_name = "level"

	for file_path in reader.get_files():
		if file_path.ends_with("/"):
			if !DirAccess.dir_exists_absolute(file_path):
				DirAccess.make_dir_recursive_absolute(file_path)
		else:
			var data = reader.read_file(file_path)
			var file = FileAccess.open("res://" + file_path, FileAccess.WRITE)
			file.store_buffer(data)
			
			if file_path.ends_with(".level"):
				level_file_path = "res://" + file_path
				level_name = file_path.get_basename()

	reader.close()

	DirAccess.copy_absolute(source_file,  save_path + "." + _get_save_extension())
	
	var current_scene_root = EditorInterface.get_edited_scene_root()
	
	if !current_scene_root:
		return ERR_CANT_CREATE
		
	var level = MFLevel.new()
	level.name = level_name
	level.set_level_file_path(level_file_path)
	current_scene_root.add_child(level)
	level.owner = current_scene_root

	return OK
