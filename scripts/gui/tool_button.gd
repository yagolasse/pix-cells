class_name ToolButton
extends Button

@export var mode: Enums.Mode
@export var filled_shape: bool = false

@onready var image_editor: ImageEditor = %ImageEditor

func _ready() -> void:
    pressed.connect(
        func():
            image_editor.mode = mode
            image_editor.filled_shape = filled_shape
    )
