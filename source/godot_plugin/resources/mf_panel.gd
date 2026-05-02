@tool
extends VBoxContainer


func _ready():
	var btn = Button.new()
	btn.text = "Create MFLevel"
	btn.pressed.connect(_on_create_pressed)
	add_child(btn)


func _on_create_pressed():
	var scene_root = EditorInterface.get_edited_scene_root()
	if not scene_root:
		push_warning("MFT: No scene open to add MFLevel to.")
		return

	var level = MFLevel.new()
	level.name = "MFLevel"

	var undo_redo = EditorInterface.get_editor_undo_redo()
	undo_redo.create_action("Create MFLevel")
	undo_redo.add_do_method(scene_root, "add_child", level)
	undo_redo.add_do_method(level, "set_owner", scene_root)
	undo_redo.add_undo_method(level, "queue_free")
	undo_redo.commit_action()
