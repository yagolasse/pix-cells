class_name NewCanvasDialog
extends ConfirmationDialog

@onready var app: App = get_parent()

@onready var width_spin_box: SpinBox = %WidthSpinBox
@onready var height_spin_box: SpinBox = %HeightSpinBox
@onready var file_menu_button: MenuButton = %FileButton
@onready var save_confirmation_dialog: SaveConfirmationDialog = %SaveConfirmationDialog

func _ready() -> void:
	visible = false
	
	confirmed.connect(_on_confirmed)
	file_menu_button.get_popup().id_pressed.connect(_on_file_menu_button_id_pressed)

func _on_confirmed() -> void:
	var width := int(width_spin_box.value)
	var height := int(height_spin_box.value)
	app.setup_blank_image(width, height)

func _on_file_menu_button_id_pressed(id: int) -> void:
	if id == Enums.FileMenuButton.NEW:
		if app.image_has_been_edited():
			save_confirmation_dialog.save_file_mode = Enums.SaveFileMode.NEW_FILE
			save_confirmation_dialog.popup_centered()
		else:
			popup_centered()
