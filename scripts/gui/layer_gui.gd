class_name LayerGUI
extends Control

const ID_META := &"id_meta"

@export var layer_button_packed_scene: PackedScene
@export var layer_button_group: ButtonGroup

@onready var image_editor: ImageEditor = %ImageEditor

func _ready() -> void:
	image_editor.layer_added.connect(_on_image_editor_layer_added)
	image_editor.layer_deleted.connect(_on_image_editor_layer_deleted)
	image_editor.active_layer_changed.connect(_on_image_editor_active_layer_changed)
	
	var layer_button := get_child(0) as Button
	layer_button.button_group = layer_button_group
	layer_button.pressed.connect(func(): _on_layer_button_pressed(layer_button.get_index()))

func _on_layer_button_pressed(index: int) -> void:
	image_editor.active_layer_index = index

func _on_image_editor_layer_added(index: int) -> void:
	var layer_button := layer_button_packed_scene.instantiate() as Button
	var layer := image_editor.layers[index]
	layer_button.text = layer.name
	layer_button.button_group = layer_button_group
	layer_button.pressed.connect(func(): _on_layer_button_pressed(layer_button.get_index()))
	add_child(layer_button)
	move_child(layer_button, index)

func _on_image_editor_layer_deleted(index: int) -> void:
	get_child(index).queue_free()

func _on_image_editor_active_layer_changed(index: int) -> void:
	for _i in 2:
		await get_tree().process_frame
	get_child(index).button_pressed = true
