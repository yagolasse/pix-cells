class_name SaveFileDialog
extends FileDialog

@onready var app: App = get_parent()

@onready var file_menu_button: MenuButton = %FileButton
@onready var open_file_dialog: FileDialog = %OpenFileDialog
@onready var new_canvas_dialog: ConfirmationDialog = %NewCanvasDialog

var save_file_mode: Enums.SaveFileMode = Enums.SaveFileMode.NONE

func _ready() -> void:
	visible = false
	
	confirmed.connect(_handle_next_state)
	
	file_selected.connect(_on_file_selected)
	file_menu_button.get_popup().id_pressed.connect(_on_file_menu_button_id_pressed)

func _on_file_selected(path: String) -> void:
	var err := app.save_image_as(path)
	
	if err:
		printerr("Error saving, ", error_string(err))
	else:
		app.update_image_local_cache(path)
		_handle_next_state()

func _handle_next_state() -> void:
	hide()
	
	match save_file_mode:
		Enums.SaveFileMode.OPEN_FILE:
			open_file_dialog.popup_centered()
		Enums.SaveFileMode.NEW_FILE:
			new_canvas_dialog.popup_centered()
		Enums.SaveFileMode.EXIT:
			get_tree().quit()

func _on_file_menu_button_id_pressed(id: int) -> void:
	if id == Enums.FileMenuButton.SAVE_AS or id == Enums.FileMenuButton.SAVE:
		filters = PackedStringArray(["*.pix"])
	elif id == Enums.FileMenuButton.EXPORT_PNG:
		filters = PackedStringArray(["*.png"])
	
	if id == Enums.FileMenuButton.EXPORT_PNG or id == Enums.FileMenuButton.SAVE_AS or (id == Enums.FileMenuButton.SAVE and not app.file_exists()):
		popup_centered()
