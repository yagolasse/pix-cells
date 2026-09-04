class_name MiniPreview
extends TextureRect

@onready var image_editor: ImageEditor = %ImageEditor
@onready var preview_popup: Popup = %PreviewPopup

var preview_texture: ImageTexture

func _ready() -> void:
	await image_editor.ready
	preview_texture = ImageTexture.create_from_image(image_editor.image)
	texture = preview_texture
	
	preview_popup.visible = true

func _process(_delta: float) -> void:
	if preview_texture:
		preview_texture.update(image_editor.image)
