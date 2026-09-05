class_name ImageEditor
extends Control

enum Mode {
	BRUSH,
	LINE,
	PAINT_BUCKET,
	SHAPE_SQUARE,
	SHAPE_CIRCLE,
	MOVE,
	ERASER,
	EYEDROPPER,
	SELECTION_SQUARE,
}

enum DisplayMode {
	PREVIEW,
	ACTUAL,
	NONE
}

@export var cursor_icons: Dictionary[Mode, Texture2D]

@onready var color_picker: ColorPicker = %ColorPicker

@onready var canvas: TextureRect = %DrawingCanvas
@onready var preview_canvas: TextureRect = %PreviewCanvas

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

@onready var size_slider: SpinBox = %SizeSlider

var image: Image
var editor_texture: ImageTexture

var preview_image: Image
var preview_editor_texture: ImageTexture

var sprite_size: Vector2i = Vector2i(64, 64)

var mouse_position: Vector2
var previous_mouse_position: Vector2

var stroke_started_outside_canvas: bool = false

var active_color: Color
var primary_color: Color
var secondary_color: Color
var preview_color: Color:
	get: return Color(active_color, active_color.a * 0.7)

var primary_selected: bool = true

var pressing_point: Vector2
var debug_pressing_point: Vector2
var running_paint_bucket: bool = false
var drag_started: bool = false

var filled_shape: bool = false

var mode: Mode = Mode.BRUSH:
	set(value):
		mode = value
		if tool_brush_size.has(mode):
			size_slider.value = tool_brush_size[mode]

var tool_brush_size: Dictionary[Mode, int] = {
	Mode.BRUSH: 1,
	Mode.ERASER: 1,
}

var brush_size: int:
	get: return tool_brush_size[mode] if tool_brush_size.has(mode) else 1

func _draw() -> void:
	var rect := canvas.get_rect()
	draw_rect(rect, Color.WHITE, false)

func _ready() -> void:
	Input.use_accumulated_input = false
	
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
	
	size_slider.value_changed.connect(_on_size_slider_value_changed)
	
	color_picker.color_changed.connect(_on_color_picker_color_changed)
	primary_color = color_picker.color
	active_color = primary_color
	
	image = Image.create_empty(sprite_size.x, sprite_size.y, false, Image.FORMAT_RGBA8)
	preview_image = Image.create_empty(sprite_size.x, sprite_size.y, false, Image.FORMAT_RGBA8)
	
	image.fill(Color.TRANSPARENT)
	
	editor_texture = ImageTexture.create_from_image(image)
	preview_editor_texture = ImageTexture.create_from_image(preview_image)
	
	canvas.texture = editor_texture
	preview_canvas.texture = preview_editor_texture

	scale *= 10

func _process(_delta: float) -> void:
	if (get_parent() as Control).get_global_rect().has_point(get_global_mouse_position()):
		_on_mouse_entered()
	else:
		_on_mouse_exited()
	
	if mode == Mode.ERASER:
		active_color = Color.TRANSPARENT
	else:
		active_color = primary_color if primary_selected else secondary_color
	
	preview_image.fill(Color.TRANSPARENT)
	
	if not _update_mouse_state(): return
	
	match mode:
		Mode.BRUSH, Mode.ERASER:
			_handle_brush_mode()
		Mode.LINE, Mode.SHAPE_SQUARE, Mode.SHAPE_CIRCLE:
			_handle_shape_mode()
		Mode.PAINT_BUCKET:
			_handle_paint_bucket_mode()
		Mode.MOVE:
			_handle_move_mode()
		Mode.EYEDROPPER:
			_handle_eyedropper_mode()
		Mode.SELECTION_SQUARE:
			_handle_square_selection_mode()
	
	if Input.is_action_just_pressed("clear"):
		for x in range(size.x):
			image.fill_rect(Rect2i(0, 0, sprite_size.x, sprite_size.y), Color.TRANSPARENT)
	
	editor_texture.update(image)
	preview_editor_texture.update(preview_image)
	
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

func _on_mouse_entered() -> void:
	var hotspot := Vector2(12, 12)
	
	match mode:
		Mode.ERASER, Mode.EYEDROPPER:
			hotspot = Vector2(4, 20)
	
	Input.set_custom_mouse_cursor(cursor_icons.get(mode), Input.CURSOR_ARROW, hotspot)

func _on_mouse_exited() -> void:
	Input.set_custom_mouse_cursor(null)

func _on_brush_button_pressed() -> void:
	mode = Mode.BRUSH

func _on_line_button_pressed() -> void:
	mode = Mode.LINE

func _on_paint_bucket_button_pressed() -> void:
	mode = Mode.PAINT_BUCKET

func _on_shape_square_button_pressed() -> void:
	mode = Mode.SHAPE_SQUARE
	filled_shape = false

func _on_shape_filled_square_button_pressed() -> void:
	mode = Mode.SHAPE_SQUARE
	filled_shape = true

func _on_shape_circle_button_pressed() -> void:
	mode = Mode.SHAPE_CIRCLE
	filled_shape = false

func _on_shape_filled_circle_button_pressed() -> void:
	mode = Mode.SHAPE_CIRCLE
	filled_shape = true

func _on_move_button_pressed() -> void:
	mode = Mode.MOVE

func _on_eraser_button_pressed() -> void:
	mode = Mode.ERASER

func _on_eyedropper_button_pressed() -> void:
	mode = Mode.EYEDROPPER

func _on_selection_square_button_pressed() -> void:
	mode = Mode.SELECTION_SQUARE

func _on_size_slider_value_changed(value: float) -> void:
	tool_brush_size[mode] = int(value)

func _update_mouse_state() -> bool:
	var mouse_released := Input.is_action_just_released("drag_start") or Input.is_action_just_released("paint_primary_color")
	
	# Mouse released, reset outside canvas flag
	if mouse_released and stroke_started_outside_canvas:
		stroke_started_outside_canvas = false
		return false
	
	var mouse_just_pressed := Input.is_action_just_pressed("drag_start") or Input.is_action_just_pressed("paint_primary_color")
	
	# Mouse outside canvas
	if mouse_just_pressed and not get_parent().get_global_rect().has_point(get_global_mouse_position()):
		stroke_started_outside_canvas = true
	
	if stroke_started_outside_canvas: return false
	
	previous_mouse_position = mouse_position
	mouse_position = canvas.get_local_mouse_position()
	
	return true

func _handle_brush_mode() -> void:
	var points_to_paint: Array[Vector2i]
	var grid_mouse_position := Vector2i(mouse_position)
	
	if Input.is_action_pressed("paint_primary_color"):
		points_to_paint.push_back(grid_mouse_position)
		
		if (previous_mouse_position - mouse_position).length() > 1:
			points_to_paint.append_array(Geometry2D.bresenham_line(previous_mouse_position, mouse_position))
	
	for point in points_to_paint:
		Painter.paint_pixel(image, point, brush_size, active_color)

	Painter.paint_pixel(preview_image, grid_mouse_position, brush_size, preview_color)

func _handle_paint_bucket_mode() -> void:
	if running_paint_bucket: return
	
	var canvas_rect := Rect2i(Vector2i.ZERO, sprite_size)
	if not canvas_rect.has_point(mouse_position): return
	
	var point_color := image.get_pixelv(mouse_position)
	
	if Input.is_action_just_pressed("paint_primary_color"):
		if point_color == active_color:
			return
			
		var initial_time := Time.get_ticks_msec()
	
		Painter.flood_fill(image, mouse_position, active_color)
			
		print("Flood fill time %d ms" % [Time.get_ticks_msec() - initial_time])

func _handle_shape_mode() -> void:
	if Input.is_action_just_pressed("drag_start"):
		pressing_point = mouse_position
		drag_started = true
	
	if Input.is_action_just_pressed("drag_cancel"):
		drag_started = false
	
	if drag_started:
		var image_to_paint: Image
		var color_to_paint: Color
		
		if Input.is_action_pressed("drag_start"):
			image_to_paint = preview_image
			color_to_paint = preview_color
		elif Input.is_action_just_released("drag_start"):
			image_to_paint = image
			color_to_paint = active_color
			drag_started = false
		
		var force_ratio := Input.is_action_pressed("symmetric_shape")
		
		match mode:
			Mode.SHAPE_SQUARE:
				Painter.paint_rect(image_to_paint, pressing_point, mouse_position, color_to_paint, filled_shape, force_ratio)
			Mode.SHAPE_CIRCLE:
				Painter.paint_ellipse(image_to_paint, mouse_position, pressing_point, color_to_paint, filled_shape, force_ratio)
			Mode.LINE:
				for point in Geometry2D.bresenham_line(pressing_point, mouse_position):
					Painter.paint_pixel(image_to_paint, point, brush_size, color_to_paint)

func _handle_move_mode() -> void:
	if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		pass

func _handle_eyedropper_mode() -> void:
	if Input.is_action_just_pressed("paint_primary_color") and Painter.image_has_point(image, int(mouse_position.x), int(mouse_position.y)):
		var cursor_color := image.get_pixel(int(mouse_position.x), int(mouse_position.y))
		
		if not is_zero_approx(cursor_color.a):
			primary_color = cursor_color
			color_picker.color = primary_color

func _handle_square_selection_mode() -> void:
	pass

func set_new_image(new_image: Image) -> void:
	image = new_image
	sprite_size = image.get_size()
	editor_texture = ImageTexture.create_from_image(image)
	canvas.texture = editor_texture
	preview_image = Image.create_empty(image.get_width(), image.get_height(), false, Image.FORMAT_RGBA8)
	preview_image.fill(Color.TRANSPARENT)
	preview_editor_texture = ImageTexture.create_from_image(preview_image)
	preview_canvas.texture = preview_editor_texture
	
	await get_tree().process_frame
	
	offset_left = 0
	offset_top = 0
	offset_right = 0
	offset_bottom = 0
