class_name App
extends Control

static var instance: App

const EXIT_CONFIRMATION_SAVE_ACTION := &"save"
const WINDOW_TITLE = "pix-cells - %s%s"

@onready var image_editor: ImageEditor = %ImageEditor
@onready var file_menu_button: MenuButton = %FileButton
@onready var save_confirmation_dialog: SaveConfirmationDialog = %SaveConfirmationDialog

var file_path: String
var reference_layers: Array[Layer]
var dialogs_open: bool = false

func _ready() -> void:
	instance = self

	get_tree().set_auto_accept_quit(false)
	
	file_menu_button.get_popup().id_pressed.connect(_on_file_menu_button_id_pressed)
	
	get_window().title = WINDOW_TITLE % ["Untitled", ""]
	
	var width := 32
	var height := 32
	
	image_editor.create_new_image(width, height)
	
	reference_layers = Layer.deep_duplicate_array(image_editor.layers)

func _process(_delta: float) -> void:
	if not image_editor.image: return
	
	image_editor.process_mode = Node.PROCESS_MODE_DISABLED if dialogs_open else Node.PROCESS_MODE_INHERIT
	
	var difference = image_has_been_edited()
	
	var format: Array[String] = []
	
	if file_path: format.push_back(file_path.get_file())
	else: format.push_back("Untitled")
	
	if difference: format.push_back("*")
	else: format.push_back("")
	
	get_window().title = WINDOW_TITLE % format

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		_on_close_request()

func _on_close_request() -> void:
	var difference = image_has_been_edited()

	if difference:
		save_confirmation_dialog.save_file_mode = Enums.SaveFileMode.EXIT
		save_confirmation_dialog.popup_centered()
	else:
		get_tree().quit()

func _on_file_menu_button_id_pressed(id: int) -> void:
	match id:
		Enums.FileMenuButton.SAVE:
			if image_has_been_edited() and file_exists():
				export_current_image_as_png(file_path)
				update_image_local_cache(file_path)
		Enums.FileMenuButton.EXIT:
			_on_close_request()

func setup_blank_image(width: int, height: int) -> void:
	image_editor.create_new_image(width, height)
	
	reference_layers = Layer.deep_duplicate_array(image_editor.layers)

func setup_new_image(new_file_path: String) -> void:
	match new_file_path.get_extension():
		"png", "PNG":
			var new_image: Image = Image.load_from_file(new_file_path)
			if new_image:
				file_path = new_file_path
				
				var image_to_edit := Image.create_empty(new_image.get_width(), new_image.get_height(), false, Image.FORMAT_RGBA8)
				image_to_edit.copy_from(new_image)
				
				image_editor.set_new_image(image_to_edit)
				
				reference_layers = Layer.deep_duplicate_array(image_editor.layers)
			else:
				printerr("Failed to read file: %s" % [new_file_path])
		"pix":
			var file := FileAccess.open(new_file_path, FileAccess.READ)
			
			var json_string := file.get_as_text()
			
			print("Loading file data: " + json_string)
			
			var json_data := JSON.parse_string(json_string) as Dictionary
			file.close()

			if json_data == null:
				push_error("Failed to parse JSON.")
				return

			var layers_data := json_data["data"] as Array
			var layers: Array[Layer] = []
			
			for i in layers_data.size():
				var layer := Layer.new()
				layer.name = "Layer %d" % [i + 1]
				layer.image = Image.create_from_data(
					json_data["width"],
					json_data["height"],
					json_data["mipmaps"],
					json_data["format"],
					Marshalls.base64_to_raw(layers_data[i])
				)
				layers.push_back(layer)
			
			reference_layers = layers
			
			image_editor.set_from_pix_file(layers)
			
			#new_image = Image.create_from_data(
				#json_data["width"],
				#json_data["height"],
				#json_data["mipmaps"],
				#json_data["format"],
				#data
			#)
	update_image_local_cache(new_file_path)

func update_image_local_cache(new_file_path: String) -> void:
	reference_layers = Layer.deep_duplicate_array(image_editor.layers)
	file_path = new_file_path
	
	get_window().title = "pix-cells - %s" % [new_file_path.get_file().get_slice(".", 0)]

func save_image_as(new_file_path: String) -> Error:
	var layers_data: Array = image_editor.layers.map(
		func(e: Layer):
			return Marshalls.raw_to_base64(e.image.get_data())
	)
	
	#var data := image_editor.image.get_data()
	#var base64_data := Marshalls.raw_to_base64(data)

	var image_dict: Dictionary = {
		"width": image_editor.image.get_width(),
		"height": image_editor.image.get_height(),
		"format": image_editor.image.get_format(), 
		"mipmaps": image_editor.image.has_mipmaps(),
		"data": layers_data
	}

	var file := FileAccess.open(new_file_path, FileAccess.WRITE)
	var json_string := JSON.stringify(image_dict)
	
	print("Saving file data: " + json_string)
	
	file.store_string(json_string)
	file.close()
	
	return OK

func export_current_image_as_png(new_file_path: String) -> Error:
	return image_editor.display_image.save_png(new_file_path)

func image_has_been_edited() -> bool:
	if reference_layers.size() != image_editor.layers.size(): return true
	
	for i in reference_layers.size():
		if reference_layers[i].image.compute_image_metrics(image_editor.layers[i].image, false)["max"] != 0:
			return true
	
	return false

func file_exists() -> bool:
	return FileAccess.file_exists(file_path)
