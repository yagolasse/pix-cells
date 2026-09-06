class_name ImageEditor
extends Control

@export var cursor_icons: Dictionary[Enums.Mode, Texture2D]

@onready var canvas: TextureRect = %DrawingCanvas
@onready var preview_canvas: TextureRect = %PreviewCanvas
@onready var color_picker: ColorPicker = %ColorPicker
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

var initial_canvas_position: Vector2
var drag_start_mouse_position: Vector2
var drag_end_mouse_position: Vector2
var running_paint_bucket: bool = false
var drag_started: bool = false
var selection_area_defined: bool = false

var filled_shape: bool = false

var undo_redo: UndoRedo = UndoRedo.new()

var mode: Enums.Mode = Enums.Mode.BRUSH:
	set(value):
		mode = value
		selection_area_defined = false
		if tool_brush_size.has(mode):
			size_slider.value = tool_brush_size[mode]

var tool_brush_size: Dictionary[Enums.Mode, int] = {
	Enums.Mode.BRUSH: 1,
	Enums.Mode.ERASER: 1,
}

var brush_size: int:
	get: return tool_brush_size[mode] if tool_brush_size.has(mode) else 1

func _draw() -> void:
	var rect := canvas.get_rect()
	draw_rect(rect, Color.WHITE, false)
	
	if mode == Enums.Mode.SELECTION_SQUARE and (drag_started or selection_area_defined):
		var local_start_mouse_position := drag_start_mouse_position - sprite_size / 2.0
		var local_mouse_position := (mouse_position + Vector2.ONE) - sprite_size / 2.0
		var x0 := int(local_start_mouse_position.x)
		var y0 := int(local_start_mouse_position.y)
		var x1 := int(local_mouse_position.x)
		var y1 := int(local_mouse_position.y)
		
		if selection_area_defined:
			var local_end_mouse_position := drag_end_mouse_position - sprite_size / 2.0
			x1 = int(local_end_mouse_position.x)
			y1 = int(local_end_mouse_position.y)
		
		var dash_lenght := 6.0 / scale.x
		draw_dashed_line(Vector2i(x0, y0), Vector2i(x1, y0), Color.GRAY, -1.0, dash_lenght)
		draw_dashed_line(Vector2i(x1, y0), Vector2i(x1, y1), Color.GRAY, -1.0, dash_lenght)
		draw_dashed_line(Vector2i(x1, y1), Vector2i(x0, y1), Color.GRAY, -1.0, dash_lenght)
		draw_dashed_line(Vector2i(x0, y1), Vector2i(x0, y0), Color.GRAY, -1.0, dash_lenght)

func _ready() -> void:
	Input.use_accumulated_input = false

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
	if Input.is_action_just_pressed("undo") and undo_redo.has_undo():
		undo_redo.undo()
	elif Input.is_action_just_pressed("redo") and undo_redo.has_redo():
		undo_redo.redo()
	
	if (get_parent() as Control).get_global_rect().has_point(get_global_mouse_position()):
		_on_mouse_entered()
	else:
		_on_mouse_exited()
	
	if mode == Enums.Mode.ERASER:
		active_color = Color.TRANSPARENT
	else:
		active_color = primary_color if primary_selected else secondary_color
	
	preview_image.fill(Color.TRANSPARENT)
	
	if not _update_mouse_state(): return
	
	match mode:
		Enums.Mode.BRUSH, Enums.Mode.ERASER:
			_handle_brush_mode()
		Enums.Mode.LINE, Enums.Mode.SHAPE_SQUARE, Enums.Mode.SHAPE_CIRCLE:
			_handle_shape_mode()
		Enums.Mode.PAINT_BUCKET:
			_handle_paint_bucket_mode()
		Enums.Mode.MOVE:
			_handle_move_mode()
		Enums.Mode.EYEDROPPER:
			_handle_eyedropper_mode()
		Enums.Mode.SELECTION_SQUARE:
			_handle_square_selection_mode()
	
	if Input.is_action_just_pressed("clear"):
		image.fill(Color.TRANSPARENT)
	
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

func _on_mouse_entered() -> void:
	var hotspot := Vector2(12, 12)
	
	match mode:
		Enums.Mode.ERASER, Enums.Mode.EYEDROPPER:
			hotspot = Vector2(4, 20)
	
	Input.set_custom_mouse_cursor(cursor_icons.get(mode), Input.CURSOR_ARROW, hotspot)

func _on_mouse_exited() -> void:
	Input.set_custom_mouse_cursor(null)

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
		
		undo_redo.create_action("Paint Bucket")
		undo_redo.add_undo_property(image, "data", image.data)
		
		var initial_time := Time.get_ticks_msec()
	
		Painter.flood_fill(image, mouse_position, active_color)
		undo_redo.add_do_property(image, "data", image.data)
		
		print("Flood fill time %d ms" % [Time.get_ticks_msec() - initial_time])
		undo_redo.commit_action(false)

func _handle_shape_mode() -> void:
	if Input.is_action_just_pressed("drag_start"):
		drag_start_mouse_position = mouse_position
		drag_started = true
		undo_redo.create_action("Drag Drawing")
		undo_redo.add_undo_property(image, "data", image.data)
	
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
			Enums.Mode.SHAPE_SQUARE:
				Painter.paint_rect(image_to_paint, drag_start_mouse_position, mouse_position, color_to_paint, filled_shape, force_ratio)
			Enums.Mode.SHAPE_CIRCLE:
				Painter.paint_ellipse(image_to_paint, mouse_position, drag_start_mouse_position, color_to_paint, filled_shape, force_ratio)
			Enums.Mode.LINE:
				for point in Geometry2D.bresenham_line(drag_start_mouse_position, mouse_position):
					Painter.paint_pixel(image_to_paint, point, brush_size, color_to_paint)
		
		if image_to_paint == image:
			undo_redo.add_do_property(image, "data", image.data)
			undo_redo.commit_action(false)

func _handle_move_mode() -> void:
	if Input.is_action_just_pressed("drag_start"):
		drag_started = true
		drag_start_mouse_position = (get_parent() as Control).get_local_mouse_position()
		initial_canvas_position = position
	
	if Input.is_action_just_released("drag_start"):
		drag_started = false
	
	if drag_started:
		position = initial_canvas_position - drag_start_mouse_position + (get_parent() as Control).get_local_mouse_position()

func _handle_eyedropper_mode() -> void:
	if Input.is_action_just_pressed("paint_primary_color") and Painter.image_has_point(image, int(mouse_position.x), int(mouse_position.y)):
		var cursor_color := image.get_pixel(int(mouse_position.x), int(mouse_position.y))
		
		if not is_zero_approx(cursor_color.a):
			primary_color = cursor_color
			color_picker.color = primary_color

func _handle_square_selection_mode() -> void:
	if selection_area_defined: 
		pass # Move selected area
	else:
		if Input.is_action_just_pressed("drag_start"):
			drag_started = true
			drag_start_mouse_position = mouse_position - Vector2.ONE
		
		if Input.is_action_just_released("drag_start"):
			selection_area_defined = true
			drag_started = false
			drag_end_mouse_position = mouse_position + Vector2.ONE

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
