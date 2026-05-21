@tool
extends EditorPlugin

var import_plugin
var panel


func _enter_tree():
	import_plugin = preload("import_plugin.gd").new()
	add_import_plugin(import_plugin)

	panel = preload("mf_panel.gd").new()
	panel.name = "My Fantasy Tools"
	add_control_to_dock(DOCK_SLOT_LEFT_UR, panel)


func _exit_tree():
	remove_import_plugin(import_plugin)
	import_plugin = null

	remove_control_from_docks(panel)
	panel.queue_free()
	panel = null
