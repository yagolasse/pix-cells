class_name SaveConfirmationDialog
extends ConfirmationDialog

const SAVE_ACTION := &"save"

var save_file_mode: Enums.SaveFileMode = Enums.SaveFileMode.NONE

@onready var app: App = get_parent()

@onready var open_file_dialog: FileDialog = %OpenFileDialog
@onready var save_file_dialog: SaveFileDialog = %SaveFileDialog
@onready var new_canvas_dialog: ConfirmationDialog = %NewCanvasDialog

var _save_button: Button

func _ready() -> void:
	_save_button = add_button("Save", false, SAVE_ACTION)
	
	visible = false
	
	visibility_changed.connect(_on_visibility_changed)
	confirmed.connect(_handle_next_state)
	custom_action.connect(_on_custom_action)

func _on_visibility_changed() -> void:
	if visible: _save_button.grab_focus()

func _on_custom_action(action: StringName) -> void:
	if action == SAVE_ACTION:
		if app.file_exists():
			app.export_current_image_as_png(app.file_path)
			app.update_image_local_cache(app.file_path)
			_handle_next_state()
		else:
			save_file_dialog.save_file_mode = save_file_mode
			save_file_dialog.popup_centered()

func _handle_next_state() -> void:
	hide()
	
	match save_file_mode:
		Enums.SaveFileMode.OPEN_FILE:
			open_file_dialog.popup_centered()
		Enums.SaveFileMode.NEW_FILE:
			new_canvas_dialog.popup_centered()
		Enums.SaveFileMode.EXIT:
			get_tree().quit()
