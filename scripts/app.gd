class_name App
extends Control

const EXIT_CONFIRMATION_SAVE_ACTION := &"save"
const WINDOW_TITLE = "pix-cells - %s%s"

@onready var file_menu_button: MenuButton = %FileButton
@onready var open_file_dialog: FileDialog = %OpenFileDialog
@onready var save_file_dialog: FileDialog = %SaveFileDialog
@onready var exit_confirmation_dialog: ConfirmationDialog = %ExitConfirmationDialog
@onready var image_editor: ImageEditor = %ImageEditor

var file_path: String
var current_image: Image
var dialogs_open: bool = false

func _ready() -> void:
	get_tree().set_auto_accept_quit(false)

	file_menu_button.get_popup().id_pressed.connect(_on_file_menu_button_id_pressed)
	open_file_dialog.file_selected.connect(_on_open_file_dialog_file_selected)
	save_file_dialog.file_selected.connect(_on_save_file_dialog_file_selected)
	exit_confirmation_dialog.confirmed.connect(_on_exit_confirmation_dialog_confirmed)
	exit_confirmation_dialog.custom_action.connect(_on_exit_custom_action_dialog_confirmed)

	exit_confirmation_dialog.add_button("Save", false, EXIT_CONFIRMATION_SAVE_ACTION)

	get_window().title = WINDOW_TITLE % ["Untitled", ""]

	current_image = Image.create_empty(image_editor.initial_sprite_size.x, image_editor.initial_sprite_size.y, false, Image.FORMAT_RGBA8)
	current_image.fill(Color.TRANSPARENT)

func _process(_delta: float) -> void:
	dialogs_open = open_file_dialog.visible or save_file_dialog.visible or exit_confirmation_dialog.visible
	image_editor.process_mode = Node.PROCESS_MODE_DISABLED if dialogs_open else Node.PROCESS_MODE_INHERIT
	
	var difference = image_editor.image.compute_image_metrics(current_image, false)["max"]
	
	var format: Array[String] = []
	
	if file_path: format.push_back(file_path.get_file())
	else: format.push_back("Untitled")
	
	if difference: format.push_back("*")
	else: format.push_back("")
	
	get_window().title = WINDOW_TITLE % format

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		_on_close_request()

func _save_image_and_update_cache(path: String) -> int:
	var err := image_editor.image.save_png(path)

	if err:
		printerr("Error saving, ", error_string(err))
	else:
		current_image.copy_from(image_editor.image)
	return err

func _on_close_request() -> void:
	var difference = image_editor.image.compute_image_metrics(current_image, false)["max"]

	if difference:
		exit_confirmation_dialog.popup_centered()
	else:
		get_tree().quit()

func _on_file_menu_button_id_pressed(id: int) -> void:
	if dialogs_open: return
	
	match id:
		0:
			open_file_dialog.popup_centered()
		1:
			if file_path:
				_save_image_and_update_cache(file_path)
			else:
				save_file_dialog.popup_centered()
		2:
			save_file_dialog.popup_centered()
		3:
			_on_close_request()

func _on_open_file_dialog_file_selected(path: String) -> void:
	var image := Image.load_from_file(path)
	
	if image:
		file_path = path
		current_image = image
		var image_to_edit := Image.create_empty(image.get_width(), image.get_height(), false, Image.FORMAT_RGBA8)
		image_to_edit.copy_from(image)
		get_window().title = "pix-cells - %s" % [path.get_file()]
		image_editor.set_new_image(image_to_edit)
	else:
		printerr("Failed to read file: %s" % [path])

func _on_save_file_dialog_file_selected(path: String, should_quit: bool = true) -> void:
	var err := _save_image_and_update_cache(path)
	
	if err:
		printerr("Error saving, ", error_string(err))
	elif should_quit:
		get_tree().quit()
	
	file_path = path

func _on_exit_confirmation_dialog_confirmed() -> void:
		get_tree().quit() # default behavior

func _on_exit_custom_action_dialog_confirmed(action: StringName) -> void:
	if action == EXIT_CONFIRMATION_SAVE_ACTION:
		exit_confirmation_dialog.hide()
		save_file_dialog.popup_centered()
