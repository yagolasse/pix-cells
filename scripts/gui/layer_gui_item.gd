class_name LayerGUIItem
extends Panel

@export var layer_visibility_button: CheckButton 
@export var layer_button: Button 
@export var mini_preview: TextureRect

@export var background_tile_size: int = 4

var preview_image: Image
var _preview_texture: ImageTexture

func _draw() -> void:
	var tile_index := 0
	var rect := mini_preview.get_rect()
	for x in range(rect.position.x, rect.position.x + rect.size.x, background_tile_size):
		tile_index += 1
		for y in range(rect.position.y, rect.position.y + rect.size.y, background_tile_size):
			tile_index += 1
			var color := Color.GRAY if tile_index % 2 == 0 else Color.DIM_GRAY
			draw_rect(Rect2(x, y, background_tile_size, background_tile_size), color)

func _ready() -> void:
	_preview_texture = ImageTexture.create_from_image(preview_image)
	mini_preview.texture = _preview_texture

func _process(_delta: float) -> void:
	_preview_texture.update(preview_image)
