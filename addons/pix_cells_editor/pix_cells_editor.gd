@tool
extends EditorPlugin

const PixCellsPanel = preload("res://pix_cells.tscn")
var main_panel_instance: Node

func _enable_plugin() -> void:
	# Add autoloads here.
	pass

func _disable_plugin() -> void:
	# Remove autoloads here.
	pass

func _has_main_screen() -> bool:
	return true

func _make_visible(visible):
	if main_panel_instance:
		main_panel_instance.visible = visible

func _get_plugin_name():
	return "Pix Cells"

func _enter_tree() -> void:
	main_panel_instance = PixCellsPanel.instantiate()
	# Add the main panel to the editor's main viewport.
	EditorInterface.get_editor_main_screen().add_child(main_panel_instance)
	# Hide the main panel. Very much required.
	_make_visible(false)


func _exit_tree() -> void:
	if main_panel_instance:
		main_panel_instance.queue_free()
