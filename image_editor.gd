class_name ImageEditor
extends Control

enum Mode {
	BRUSH,
	ERASER,
	LINE
}

@export var color_picker: ColorPicker

@onready var canvas: TextureRect = $TextureRect

@onready var brush_button: Button = %BrushButton
@onready var eraser_button: Button = %EraserButton
@onready var line_button: Button = %LineButton

@onready var size_slider: SpinBox = %SizeSlider

var image: Image
var editor_texture: ImageTexture
@onready var sprite_size: Vector2i = Vector2i(8, 8):
	set(value):
		sprite_size = value
		size = sprite_size
		var parent_size := (get_parent() as Control).size - Vector2(32, 32)
		var initial_scale := parent_size.x / size.x if size.x > size.y else parent_size.y / size.y
		scale = Vector2.ONE * initial_scale

var is_mouse_pressed: bool
var was_mouse_pressed: bool

var mouse_position: Vector2
var previous_mouse_position: Vector2

var stroke_started_outside_canvas: bool = false

var active_color: Color
var primary_color: Color
var secondary_color: Color

var primary_selected: bool = true

var mode: Mode = Mode.BRUSH:
	set(value):
		mode = value
		size_slider.value = tool_brush_size[mode]

var tool_brush_size: Dictionary[Mode, int] = {
	Mode.BRUSH: 1, 
	Mode.ERASER: 1,
	Mode.LINE: 1
}

func _draw() -> void:
	var canvas_rect := canvas.get_rect()
	var rect := Rect2(canvas_rect.position.x, canvas_rect.position.y, canvas.size.x, canvas.size.y)
	draw_rect(rect, Color.WHITE, false)
	
	# Drawing brush preview
	
	var grid_mouse_position := Vector2i(get_parent().get_local_mouse_position())
	
	if get_parent().get_global_rect().has_point(get_global_mouse_position()):
		draw_rect(_calculate_paint_rect(Vector2i(get_local_mouse_position())), Color(active_color, 0.5))
		#match mode:
			#Mode.BRUSH:
				#draw_rect(_calculate_paint_rect(Vector2i(get_local_mouse_position())), Color(active_color, 0.5))
			#Mode.ERASER:
				#var cursor_color := get_parent().get_viewport().get_texture().get_image().get_pixelv(grid_mouse_position)
				#var luminance := cursor_color.get_luminance()
				#var border_color := Color.WHITE if luminance < 0.5 else Color.BLACK
				#draw_rect(_calculate_paint_rect(Vector2i(get_local_mouse_position())), border_color, false)

func _ready() -> void:
	Input.use_accumulated_input = false
	
	brush_button.button_pressed = true
	
	brush_button.pressed.connect(_on_brush_button_pressed)
	eraser_button.pressed.connect(_on_eraser_button_pressed)
	line_button.pressed.connect(_on_line_button_pressed)
	
	size_slider.value_changed.connect(_on_size_slider_value_changed)
	
	color_picker.color_changed.connect(_on_color_picker_color_changed)
	primary_color = color_picker.color
	active_color = primary_color
	
	@warning_ignore("integer_division")
	#position += Vector2(sprite_size / 2)
	image = Image.create_empty(int(sprite_size.x), int(sprite_size.y), false, Image.FORMAT_RGBA8)
	
	editor_texture = ImageTexture.create_from_image(image)
	
	canvas.texture = editor_texture

func _process(_delta: float) -> void:
	Debug.instance.add_debug_property("Mouse position", mouse_position)
	Debug.instance.add_debug_property("Grid Mouse position", Vector2i(get_local_mouse_position()))
	Debug.instance.add_debug_property("Is mouse pressed", is_mouse_pressed)
	Debug.instance.add_debug_property("Was mouse pressed", was_mouse_pressed)
	Debug.instance.add_debug_property("Outside Canvas", stroke_started_outside_canvas)
	
	if mode == Mode.ERASER:
		active_color = Color.TRANSPARENT
	else:
		active_color = primary_color if primary_selected else secondary_color
	
	if not _update_mouse_state(): return
	
	match mode:
		Mode.BRUSH, Mode.ERASER:
			_handle_brush_mode()
	
	if Input.is_action_just_pressed("clear"):
		for x in range(size.x):
			image.fill_rect(Rect2i(0, 0, int(sprite_size.x), int(sprite_size.y)), Color.TRANSPARENT)
	
	editor_texture.update(image)
	
	queue_redraw()

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
	var points_to_paint: Array[Vector2i] = []
	
	if is_mouse_pressed:
		points_to_paint.push_back(Vector2i(mouse_position))
		
		if was_mouse_pressed and (previous_mouse_position - mouse_position).length() > 1:
			points_to_paint.append_array(Geometry2D.bresenham_line(previous_mouse_position, mouse_position))
	
	for point in points_to_paint:
		var rect := _calculate_paint_rect(point)
		image.fill_rect(rect, active_color)

func _calculate_paint_rect(point: Vector2i) -> Rect2i:
	var current_size := tool_brush_size[mode]
	var rect_size := Vector2i(current_size, current_size)
	@warning_ignore("integer_division")
	return Rect2i(point - rect_size / 2, rect_size)

func _on_brush_button_pressed() -> void:
	mode = Mode.BRUSH

func _on_eraser_button_pressed() -> void:
	mode = Mode.ERASER

func _on_line_button_pressed() -> void:
	mode = Mode.LINE

func _on_size_slider_value_changed(value: float) -> void:
	tool_brush_size[mode] = int(value)

func set_new_image(new_image: Image) -> void:
	image = new_image
	sprite_size = image.get_size()
	editor_texture = ImageTexture.create_from_image(image)
	canvas.texture = editor_texture
