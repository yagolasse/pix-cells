class_name LayerGUI
extends Control

const ID_META := &"id_meta"

@export var layer_gui_item_packed_scene: PackedScene
@export var layer_button_group: ButtonGroup

@onready var image_editor: ImageEditor = %ImageEditor

func _ready() -> void:
	image_editor.layers_changed.connect(_on_image_editor_layers_changed)

func _on_layer_button_pressed(index: int) -> void:
	image_editor.active_layer_index = index

func _on_layer_visibility_toggle(index: int, should_show: bool) -> void:
	image_editor.layers[index].visible = should_show

func _on_image_editor_layers_changed() -> void:
	for c in get_children():
		c.queue_free()
	
	for i in image_editor.layers.size():
		add_child(_configure_item(image_editor.layers[i], i == image_editor.active_layer_index))

func _configure_item(layer: Layer, selected: bool) -> LayerGUIItem:
	var layer_item := layer_gui_item_packed_scene.instantiate() as LayerGUIItem
	layer_item.preview_image = layer.image
	layer_item.layer_button.text = layer.name
	layer_item.layer_button.button_pressed = selected
	layer_item.layer_button.button_group = layer_button_group
	layer_item.layer_button.pressed.connect(
		func(): 
			_on_layer_button_pressed(layer_item.get_index())
	)
	layer_item.layer_visibility_button.pressed.connect(
		func(): 
			_on_layer_visibility_toggle(
				layer_item.get_index(), 
				layer_item.layer_visibility_button.button_pressed
			)
	)
	return layer_item
