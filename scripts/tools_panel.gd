class_name ToolsPanel
extends Panel

@onready var image_editor: ImageEditor = %ImageEditor
@onready var brush_button: Button = %BrushButton
@onready var line_button: Button = %LineButton
@onready var paint_bucket_button: Button = %PaintBucketButton
@onready var shape_square_button: Button = %ShapeSquareButton
@onready var shape_filled_square_button: Button = %ShapeFilledSquareButton
@onready var shape_circle_button: Button = %ShapeCircleButton
@onready var shape_filled_circle_button: Button = %ShapeFilledCircleButton
@onready var move_button: Button = %MoveButton
@onready var eraser_button: Button = %EraserButton
@onready var eyedropper_button: Button = %EyedropperButton
@onready var selection_square_button: Button = %SelectionSquareButton
@onready var selection_free_button: Button = %SelectionFreeButton
@onready var color_picker: ColorPicker = %ColorPicker
@onready var size_slider: SpinBox = %SizeSlider

func _ready() -> void:
	brush_button.button_pressed = true
	
	brush_button.pressed.connect(_on_brush_button_pressed)
	line_button.pressed.connect(_on_line_button_pressed)
	paint_bucket_button.pressed.connect(_on_paint_bucket_button_pressed)
	shape_square_button.pressed.connect(_on_shape_square_button_pressed)
	shape_filled_square_button.pressed.connect(_on_shape_filled_square_button_pressed)
	shape_circle_button.pressed.connect(_on_shape_circle_button_pressed)
	shape_filled_circle_button.pressed.connect(_on_shape_filled_circle_button_pressed)
	move_button.pressed.connect(_on_move_button_pressed)
	eraser_button.pressed.connect(_on_eraser_button_pressed)
	eyedropper_button.pressed.connect(_on_eyedropper_button_pressed)
	selection_square_button.pressed.connect(_on_selection_square_button_pressed)
	selection_free_button.pressed.connect(_on_selection_free_button_pressed)
	size_slider.value_changed.connect(_on_size_slider_value_changed)
	color_picker.color_changed.connect(_on_color_picker_color_changed)


func _on_brush_button_pressed() -> void:
	image_editor.mode = Enums.Mode.BRUSH

func _on_line_button_pressed() -> void:
	image_editor.mode = Enums.Mode.LINE

func _on_paint_bucket_button_pressed() -> void:
	image_editor.mode = Enums.Mode.PAINT_BUCKET

func _on_shape_square_button_pressed() -> void:
	image_editor.mode = Enums.Mode.SHAPE_SQUARE
	image_editor.filled_shape = false

func _on_shape_filled_square_button_pressed() -> void:
	image_editor.mode = Enums.Mode.SHAPE_SQUARE
	image_editor.filled_shape = true

func _on_shape_circle_button_pressed() -> void:
	image_editor.mode = Enums.Mode.SHAPE_CIRCLE
	image_editor.filled_shape = false

func _on_shape_filled_circle_button_pressed() -> void:
	image_editor.mode = Enums.Mode.SHAPE_CIRCLE
	image_editor.filled_shape = true

func _on_move_button_pressed() -> void:
	image_editor.mode = Enums.Mode.MOVE

func _on_eraser_button_pressed() -> void:
	image_editor.mode = Enums.Mode.ERASER

func _on_eyedropper_button_pressed() -> void:
	image_editor.mode = Enums.Mode.EYEDROPPER

func _on_selection_square_button_pressed() -> void:
	image_editor.mode = Enums.Mode.SELECTION_SQUARE
	
func _on_selection_free_button_pressed() -> void:
	image_editor.mode = Enums.Mode.SELECTION_FREE

func _on_size_slider_value_changed(value: float) -> void:
	image_editor.tool_brush_size[image_editor.mode] = int(value)

func _on_color_picker_color_changed(color: Color) -> void:
	image_editor.primary_color = color
