class_name DialogWatcher
extends Node

@export var open_file_dialog: Window
@export var save_file_dialog: Window
@export var new_canvas_dialog: Window
@export var save_confirmation_dialog: Window

func any_open() -> bool:
	return open_file_dialog.visible or \
		save_file_dialog.visible or \
		new_canvas_dialog.visible or \
		save_confirmation_dialog.visible
