class_name ImageEditor
extends Control

enum Mode {
	BRUSH,
	ERASER,
	LINE
}

@export var color_picker: ColorPicker

@onready var canvas: TextureRect = $TextureRect

var image: Image
var editor_texture: ImageTexture
var sprite_size: Vector2i = Vector2i(64, 64)

var is_mouse_pressed: bool
var was_mouse_pressed: bool

var mouse_position: Vector2
var previous_mouse_position: Vector2

var stroke_started_outside_canvas: bool = false

var primary_color: Color

var mode: Mode = Mode.BRUSH

func _draw() -> void:
	var canvas_rect := canvas.get_rect()
	var rect := Rect2(canvas_rect.position.x + 1, canvas_rect.position.y + 1, canvas.size.x - 1, canvas.size.y - 1)
	draw_rect(rect, Color.WHITE, false)

func _ready() -> void:
	Input.use_accumulated_input = false
	
	color_picker.color_changed.connect(_on_color_picker_color_changed)

	@warning_ignore("integer_division")
	position += Vector2(sprite_size / 2)
	image = Image.create_empty(int(sprite_size.x), int(sprite_size.y), false, Image.FORMAT_RGBA8)
	
	editor_texture = ImageTexture.create_from_image(image)
	
	canvas.texture = editor_texture

	queue_redraw()

func _process(_delta: float) -> void:
	Debug.instance.add_debug_property("Is mouse pressed", is_mouse_pressed)
	Debug.instance.add_debug_property("Was mouse pressed", was_mouse_pressed)
	Debug.instance.add_debug_property("Outside Canvas", stroke_started_outside_canvas)

	if not _update_mouse_state(): return

	match mode:
		Mode.BRUSH:
			_handle_brush_mode()

	editor_texture.update(image)

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			scale *= 1.2
		if event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			scale /= 1.2

		queue_redraw()
	
func _on_color_picker_color_changed(color: Color) -> void:
	primary_color = color

func _update_mouse_state() -> bool:
	was_mouse_pressed = is_mouse_pressed
	is_mouse_pressed = Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	
	# Mouse released, reset outside canvas flag
	if was_mouse_pressed and not is_mouse_pressed and stroke_started_outside_canvas:
		stroke_started_outside_canvas = false
		return false

	# Mouse outside canvas
	if not was_mouse_pressed and is_mouse_pressed and not get_parent().get_global_rect().has_point(get_global_mouse_position()):
		stroke_started_outside_canvas = true

	if stroke_started_outside_canvas: return false

	previous_mouse_position = mouse_position
	mouse_position = canvas.get_local_mouse_position()

	return true

func _handle_brush_mode() -> void:
	if is_mouse_pressed:
		var brush := Vector2i(mouse_position)
		if brush.x > 0 and brush.y > 0 and brush.x < sprite_size.x and brush.y < sprite_size.y:
			image.set_pixel(brush.x, brush.y, primary_color)
	
	if was_mouse_pressed and not stroke_started_outside_canvas and (previous_mouse_position - mouse_position).length() > 1:
		var points := Geometry2D.bresenham_line(previous_mouse_position, mouse_position)

		for point in points:
			if point.x > 0 and point.y > 0 and point.x < sprite_size.x and point.y < sprite_size.y:
				image.set_pixel(point.x, point.y, primary_color)

	if Input.is_action_just_pressed("clear"):
		for x in range(size.x):
			image.fill_rect(Rect2i(0, 0, int(sprite_size.x), int(sprite_size.y)), Color.TRANSPARENT)
