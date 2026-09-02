class_name App
extends Control

@onready var open_file_button: MenuButton = %OpenFileButton
@onready var file_dialog: FileDialog = %FileDialog
@onready var image_editor: ImageEditor = %ImageEditor

func _ready() -> void:
	open_file_button.pressed.connect(_on_open_file_button_pressed)
	file_dialog.file_selected.connect(_on_file_dialog_file_selected)

func _on_open_file_button_pressed() -> void:
	file_dialog.visible = true

func _on_file_dialog_file_selected(path: String) -> void:   
	var image = Image.load_from_file(path)
	
	if image:
		image_editor.set_new_image(image)
	else:
		printerr("Failed to read file: %s" % [path])
