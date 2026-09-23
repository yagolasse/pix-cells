class_name OpenFileDialog
extends FileDialog

@onready var app: App = get_parent()

@onready var file_menu_button: MenuButton = %FileButton
@onready var save_confirmation_dialog: SaveConfirmationDialog = %SaveConfirmationDialog

func _ready() -> void:
	visible = false
	
	file_selected.connect(_on_file_selected)
	file_menu_button.get_popup().id_pressed.connect(_on_file_menu_button_id_pressed)

func _on_file_selected(path: String) -> void:
	app.setup_new_image(path)

func _on_file_menu_button_id_pressed(id: int) -> void:
	if id == Enums.FileMenuButton.OPEN:
		if app.image_has_been_edited():
			save_confirmation_dialog.save_file_mode = Enums.SaveFileMode.OPEN_FILE
			save_confirmation_dialog.popup_centered()
		else:
			popup_centered()
