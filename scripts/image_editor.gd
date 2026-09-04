class_name ImageEditor
extends Control

enum Mode {
	BRUSH,
	LINE,
	PAINT_BUCKET,
	SHAPE_SQUARE,
	SHAPE_CIRCLE,
	ERASER,
}

@onready var color_picker: ColorPicker = %ColorPicker

@onready var canvas: TextureRect = %DrawingCanvas
@onready var preview_canvas: TextureRect = %PreviewCanvas

@onready var brush_button: Button = %BrushButton
@onready var line_button: Button = %LineButton
@onready var paint_bucket_button: Button = %PaintBucketButton
@onready var shape_square_button: Button = %ShapeSquareButton
@onready var shape_circle_button: Button = %ShapeCircleButton
@onready var eraser_button: Button = %EraserButton

@onready var size_slider: SpinBox = %SizeSlider

var image: Image
var editor_texture: ImageTexture

var preview_image: Image
var preview_editor_texture: ImageTexture

@onready var sprite_size: Vector2i = Vector2i(64, 64):
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

var pressing_point: Vector2
var debug_pressing_point: Vector2
var running_paint_bucket: bool = false

var mode: Mode = Mode.BRUSH:
	set(value):
		mode = value
		if tool_brush_size.has(mode):
			size_slider.value = tool_brush_size[mode]

var tool_brush_size: Dictionary[Mode, int] = {
	Mode.BRUSH: 1,
	Mode.ERASER: 1,
}

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
	shape_circle_button.pressed.connect(_on_shape_circle_button_pressed)
	eraser_button.pressed.connect(_on_eraser_button_pressed)
	
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

func _process(_delta: float) -> void:
	#Debug.instance.add_debug_property("Canvas Rect", canvas.get_rect())
	#Debug.instance.add_debug_property("Mouse position", mouse_position)
	#Debug.instance.add_debug_property("Grid Mouse position", get_local_mouse_position())
	#Debug.instance.add_debug_property("Is mouse pressed", is_mouse_pressed)
	#Debug.instance.add_debug_property("Was mouse pressed", was_mouse_pressed)
	#Debug.instance.add_debug_property("Outside Canvas", stroke_started_outside_canvas)
	#var distance := mouse_position - pressing_point if pressing_point > mouse_position else pressing_point - mouse_position
	#Debug.instance.add_debug_property("Distance", distance)
	if mode == Mode.ERASER:
		active_color = Color.TRANSPARENT
	else:
		active_color = primary_color if primary_selected else secondary_color
	
	preview_image.fill(Color.TRANSPARENT)
	
	if not _update_mouse_state(): return
	
	match mode:
		Mode.BRUSH, Mode.ERASER:
			_handle_brush_mode()
		Mode.LINE:
			_handle_line_mode()
		Mode.PAINT_BUCKET:
			_handle_paint_bucket_mode()
		Mode.SHAPE_SQUARE:
			_handle_shape_square_mode()
		Mode.SHAPE_CIRCLE:
			_handle_shape_circle_mode()
	
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

func _on_brush_button_pressed() -> void:
	mode = Mode.BRUSH

func _on_line_button_pressed() -> void:
	mode = Mode.LINE

func _on_paint_bucket_button_pressed() -> void:
	mode = Mode.PAINT_BUCKET

func _on_shape_square_button_pressed() -> void:
	mode = Mode.SHAPE_SQUARE

func _on_shape_circle_button_pressed() -> void:
	mode = Mode.SHAPE_CIRCLE

func _on_eraser_button_pressed() -> void:
	mode = Mode.ERASER

func _on_size_slider_value_changed(value: float) -> void:
	tool_brush_size[mode] = int(value)

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
	var points_to_paint: Array[Vector2i]
	var grid_mouse_position := Vector2i(mouse_position)
	var brush_size := tool_brush_size[mode] if tool_brush_size.has(mode) else 1
	
	if is_mouse_pressed:
		points_to_paint.push_back(grid_mouse_position)
		
		if was_mouse_pressed and (previous_mouse_position - mouse_position).length() > 1:
			points_to_paint.append_array(Geometry2D.bresenham_line(previous_mouse_position, mouse_position))
	
	for point in points_to_paint:
		Painter.paint_pixel(image, point.x, point.y, brush_size, active_color)

	Painter.paint_pixel(preview_image, grid_mouse_position.x, grid_mouse_position.y, brush_size, Color(active_color, 0.5))

func _handle_line_mode() -> void:
	if is_mouse_pressed and not was_mouse_pressed:
		pressing_point = mouse_position
	
	if was_mouse_pressed and not is_mouse_pressed:
		var points_to_paint := Geometry2D.bresenham_line(pressing_point, mouse_position)
		
		for point in points_to_paint:
			var rect := _calculate_paint_rect(point)
			image.fill_rect(rect, active_color)
	
	if is_mouse_pressed:
		var preview_line := Geometry2D.bresenham_line(pressing_point, mouse_position)
	
		for point in preview_line:
			var rect := _calculate_paint_rect(point)
			preview_image.fill_rect(rect, Color(active_color, 0.5))

## Line span flood fill algorithm.
## Taken from https://en.wikipedia.org/wiki/Flood_fill#Span_filling.
func _handle_paint_bucket_mode() -> void:
	if running_paint_bucket: return
	
	var canvas_rect := Rect2i(Vector2i.ZERO, sprite_size)
	if not canvas_rect.has_point(mouse_position): return
	
	var point_color := image.get_pixelv(mouse_position)
	
	if is_mouse_pressed and not was_mouse_pressed:
		if point_color == active_color:
			return
			
		var initial_time := Time.get_ticks_msec()
	
		var point_queue: Array[PointRange] = [
			PointRange.from(int(mouse_position.x), int(mouse_position.x), int(mouse_position.y), 1),
			PointRange.from(int(mouse_position.x), int(mouse_position.x), int(mouse_position.y) - 1, -1)
		]
	
		while not point_queue.is_empty():
			var p := point_queue.pop_back() as PointRange
			var x := p.x1
			var x1 := p.x1
			
			if canvas_rect.has_point(Vector2i(x, p.y)) and image.get_pixel(x, p.y) == point_color:
				while canvas_rect.has_point(Vector2i(x - 1, p.y)) and image.get_pixel(x - 1, p.y) == point_color:
					image.set_pixel(x - 1, p.y, active_color)
					x -= 1
				if x < x1:
					point_queue.push_back(PointRange.from(x, x1 - 1, p.y - p.dy, -p.dy))
			while x1 <= p.x2:
				while canvas_rect.has_point(Vector2i(x1, p.y)) and image.get_pixel(x1, p.y) == point_color:
					image.set_pixel(x1, p.y, active_color)
					x1 += 1
				if x1 > x:
					point_queue.push_back(PointRange.from(x, x1 - 1, p.y + p.dy, p.dy))
				if x1 - 1 > p.x2:
					point_queue.push_back(PointRange.from(p.x2 + 1, x1 - 1, p.y - p.dy, -p.dy))
				x1 += 1
				while x1 <= p.x2 and (not canvas_rect.has_point(Vector2i(x1, p.y)) or not image.get_pixel(x1, p.y) == point_color):
					x1 += 1
				x = x1
			
		print("Flood fill time %d ms" % [Time.get_ticks_msec() - initial_time])

func _handle_shape_square_mode() -> void:
	if is_mouse_pressed and not was_mouse_pressed:
		pressing_point = mouse_position
	
	var draw_lines: Array[Vector2i] = []
	
	var corners: Array[Vector2i]
	
	if Input.is_action_pressed("symmetric_shape"):
		var distance := mouse_position - pressing_point if pressing_point > mouse_position else pressing_point - mouse_position
		var offset := distance.x if absf(distance.x) < absf(distance.y) else distance.y
		
		if pressing_point < mouse_position: offset *= -1
 
		corners.append_array([
			Vector2i(int(pressing_point.x), int(pressing_point.y)),
			Vector2i(int(pressing_point.x + offset), int(pressing_point.y)),
			Vector2i(int(pressing_point.x + offset), int(pressing_point.y + offset)),
			Vector2i(int(pressing_point.x), int(pressing_point.y + offset)),
		])
	else:
		corners.append_array([
			Vector2i(int(pressing_point.x), int(pressing_point.y)),
			Vector2i(int(mouse_position.x), int(pressing_point.y)),
			Vector2i(int(mouse_position.x), int(mouse_position.y)),
			Vector2i(int(pressing_point.x), int(mouse_position.y))
		])
	
	for i in corners.size():
		var j := i - 1 if i > 0 else corners.size() - 1
		draw_lines.append_array(Geometry2D.bresenham_line(corners[j], corners[i]))
	
	for point in draw_lines:
		var rect := _calculate_paint_rect(point)
		if is_mouse_pressed:
			preview_image.fill_rect(rect, Color(active_color, 0.5))
		elif was_mouse_pressed: # Just released
			image.fill_rect(rect, active_color)

func _handle_shape_circle_mode() -> void:
	if is_mouse_pressed and not was_mouse_pressed:
		pressing_point = mouse_position
	
	if not is_mouse_pressed: return

	var points: Array[Vector2i] = []

	var radius := (pressing_point - mouse_position).length() / 2.0
	var center := (pressing_point) if pressing_point < mouse_position else mouse_position

	var top = ceil(center.y - radius)
	var bottom = floor(center.y + radius)
	var left = ceil(center.x - radius)
	var right = floor(center.x + radius)

	var rect := Rect2i(center.x, center.y, absi(mouse_position.x - pressing_point.x), absi(mouse_position.y - pressing_point.y))
		# Calculate sub-pixel radii
	var a: float = rect.size.x / 2.0
	var b: float = rect.size.y / 2.0
	
	# Calculate sub-pixel centers
	var cx: float = rect.position.x + a
	var cy: float = rect.position.y + b

	var img_w: int = image.get_width()
	var img_h: int = image.get_height()

	# Helper to safely draw mirrored coordinates
	var plot = func(x_offset: int, y_offset: int):
		# Mirror across the true bounding rect boundaries
		var x1: int = rect.position.x + x_offset
		var x2: int = rect.position.x + rect.size.x - 1 - x_offset
		var y1: int = rect.position.y + y_offset
		var y2: int = rect.position.y + rect.size.y - 1 - y_offset

		var local_points = [
			Vector2i(x1, y1), Vector2i(x2, y1),
			Vector2i(x1, y2), Vector2i(x2, y2)
		]
		for p in local_points:
			if p.x >= 0 and p.x < img_w and p.y >= 0 and p.y < img_h:
				preview_image.set_pixelv(p, Color(active_color, 0.5))

	# Region 1: Step along X
	var x: int = 0
	var y: int = int(b)
	
	var a2: float = a * a
	var b2: float = b * b
	
	# Initial decision parameter for region 1
	# Evaluates the ellipse function at the mid-point of the next pixels
	var p: float = b2 - (a2 * b) + (0.25 * a2)
	var dx: float = 2.0 * b2 * x
	var dy: float = 2.0 * a2 * y

	while dx < dy:
		plot.call(x, int(b) - y)
		x += 1
		dx += 2.0 * b2
		if p < 0:
			p += b2 + dx
		else:
			y -= 1
			dy -= 2.0 * a2
			p += b2 + dx - dy

	# Region 2: Step along Y
	p = b2 * ((x + 0.5) * (x + 0.5)) + a2 * ((y - 1) * (y - 1)) - a2 * b2
	while y >= 0:
		plot.call(x, int(b) - y)
		y -= 1
		dy -= 2.0 * a2
		if p > 0:
			p += a2 - dy
		else:
			x += 1
			dx += 2.0 * b2
			p += a2 - dy + dx

	# # Loop through the bounding box grid
	# for y in range(rect.position.y, rect.position.y + rect.size.y):
	# 	for x in range(rect.position.x, rect.position.x + rect.size.x):
	# 		# Check boundaries to avoid crashing out of bounds
	# 		if x < 0 or x >= image.get_width() or y < 0 or y >= image.get_height():
	# 			continue
				
	# 		# Standard Ellipse Equation: (x-cx)^2/rx^2 + (y-cy)^2/ry^2 <= 1
	# 		# We use normalized offsets from the center
	# 		var dx: float = (x + 0.5 - cx) / rx
	# 		var dy: float = (y + 0.5 - cy) / ry
	# 		var value: float = (dx * dx) + (dy * dy)
			
	# 		# For a solid/filled ellipse, check if it's inside the boundary (<= 1.0)
	# 		# To create an outline, check if it is within a small threshold (e.g., between 0.9 and 1.0)
	# 		if is_mouse_pressed and value <= 1.0 and value >= 0.8:
	# 			preview_image.set_pixel(x, y, Color(active_color, 0.5))

	# for x in range(left, right):
	# 	for y in range(top, bottom):
	# 		if _point_inside_circle(center, Vector2i(x, y), radius):
	# 			points.push_back(Vector2i(x, y))

	# if is_mouse_pressed:
	# 	for point in points:
	# 		preview_image.set_pixelv(point, Color(active_color, 0.5))

func _point_inside_circle(center: Vector2i, tile: Vector2i, radius: float) -> bool:
	var distance := center - tile
	return distance.length_squared() < radius * radius

func _calculate_paint_rect(point: Vector2i) -> Rect2i:
	var current_size := tool_brush_size[mode] if tool_brush_size.has(mode) else 1
	var rect_size := Vector2i(current_size, current_size)
	@warning_ignore("integer_division")
	return Rect2i(point - rect_size / 2, rect_size)

func set_new_image(new_image: Image) -> void:
	image = new_image
	sprite_size = image.get_size()
	editor_texture = ImageTexture.create_from_image(image)
	canvas.texture = editor_texture
	
	await get_tree().process_frame
	
	offset_left = 0
	offset_top = 0
	offset_right = 0
	offset_bottom = 0

class PointRange extends RefCounted:
	var x1: int
	var x2: int
	var y: int
	var dy: int
	
	static func from(_x1: int, _x2: int, _y: int, _dy: int) -> PointRange:
		var p := PointRange.new()
		p.x1 = _x1
		p.x2 = _x2
		p.y = _y
		p.dy = _dy
		return p
