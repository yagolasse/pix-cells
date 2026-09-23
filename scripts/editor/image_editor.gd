class_name ImageEditor
extends Control

@export var cursor_icons: Dictionary[Enums.Mode, Texture2D]

@onready var canvas: TextureRect = %DrawingCanvas
@onready var preview_canvas: TextureRect = %PreviewCanvas
@onready var color_picker: ColorPicker = %ColorPicker
@onready var size_slider: SpinBox = %SizeSlider
@onready var mouse_position_label: Label = %MousePositionLabel

var image: Image
var clipboard_image: Image
var editor_texture: ImageTexture

var preview_image: Image
var preview_editor_texture: ImageTexture

var mouse_position: Vector2
var previous_mouse_position: Vector2

var stroke_started_outside_canvas: bool = false

var active_color: Color
var primary_color: Color
var secondary_color: Color
var preview_color: Color:
	get: return Color(active_color, active_color.a * 0.7)

var primary_selected: bool = true

var initial_move_position: Vector2
var drag_start_mouse_position: Vector2
var drag_end_mouse_position: Vector2
var clipboard_rect: Rect2i
var running_paint_bucket: bool = false
var drag_started: bool = false
var selection_area_defined: bool = false
var selection_polygon_points: Array[Vector2i]

var filled_shape: bool = false

var undo_redo: UndoRedo = UndoRedo.new()

var mode: Enums.Mode = Enums.Mode.BRUSH:
	set(value):
		if mode == value: return
		mode = value
	
		selection_area_defined = false

		if is_mouse_inside_canvas:
			_on_mouse_entered()

		if tool_brush_size.has(mode):
			size_slider.visible = true
			size_slider.value = tool_brush_size[mode]
		else:
			size_slider.visible = false

var tool_brush_size: Dictionary[Enums.Mode, int] = {
	Enums.Mode.BRUSH: 1,
	Enums.Mode.ERASER: 1,
}

var brush_size: int:
	get: return tool_brush_size[mode] if tool_brush_size.has(mode) else 1

@onready var is_mouse_inside_canvas: bool = false:
	get():
		return is_mouse_inside_canvas
	set(value): 
		if value == is_mouse_inside_canvas: return
		is_mouse_inside_canvas = value
		if is_mouse_inside_canvas: _on_mouse_entered()
		else: _on_mouse_exited()

var background_tile_size: int = 4

func _draw() -> void:
	var rect := canvas.get_rect()
	draw_rect(rect, Color.WHITE, false)

	if image:
		var tile_index := 0
		for x in range(0, image.get_width(), background_tile_size):
			tile_index += 1
			for y in range(0, image.get_height(), background_tile_size):
				tile_index += 1
				var color := Color.GRAY if tile_index % 2 == 0 else Color.DIM_GRAY
				draw_rect(Rect2(x - image.get_width() / 2.0, y - image.get_height() / 2.0, background_tile_size, background_tile_size), color)

	
	if mode == Enums.Mode.SELECTION_SQUARE and (drag_started or selection_area_defined):
		@warning_ignore_start("integer_division")
		var x0 := clipboard_rect.position.x - image.get_size().x / 2
		var y0 := clipboard_rect.position.y - image.get_size().y / 2
		@warning_ignore_restore("integer_division")
		
		var x1 := x0 + clipboard_rect.size.x
		var y1 := y0 + clipboard_rect.size.y
		
		var dash_lenght := 6.0 / scale.x
		draw_dashed_line(Vector2i(x0, y0), Vector2i(x1, y0), Color.GRAY, -1.0, dash_lenght)
		draw_dashed_line(Vector2i(x1, y0), Vector2i(x1, y1), Color.GRAY, -1.0, dash_lenght)
		draw_dashed_line(Vector2i(x1, y1), Vector2i(x0, y1), Color.GRAY, -1.0, dash_lenght)
		draw_dashed_line(Vector2i(x0, y1), Vector2i(x0, y0), Color.GRAY, -1.0, dash_lenght)
		
	if mode == Enums.Mode.SELECTION_FREE and (drag_started or selection_area_defined) and not selection_polygon_points.is_empty():
		@warning_ignore_start("integer_division")
		var local_points := selection_polygon_points.map(func(e): return e - image.get_size() / 2)
		@warning_ignore_restore("integer_division")
		
		var points_size := local_points.size()
		var first_point := local_points[0] as Vector2i
		var last_point := local_points[points_size - 1] as Vector2i
		var complementary_points := [] if abs((first_point - last_point).length()) < 1 else Geometry2D.bresenham_line(last_point, first_point)
		draw_polygon(local_points + complementary_points, [Color.GRAY])

func _ready() -> void:
	Input.use_accumulated_input = false

	RenderingServer.canvas_item_set_custom_rect(get_canvas_item(), true, get_viewport_rect())

	size_slider.value_changed.connect(_on_size_slider_value_changed)
	color_picker.color_changed.connect(_on_color_picker_color_changed)

	primary_color = Color.BLACK
	active_color = Color.BLACK
	color_picker.color = Color.BLACK

func _process(_delta: float) -> void:
	if not image or not preview_image or not clipboard_image: return
	
	var grid_mouse_position := Vector2i(canvas.get_local_mouse_position())
	mouse_position_label.text = "x: %d, y: %d" % [grid_mouse_position.x, grid_mouse_position.y]
	
	var mouse_pressed := Input.is_action_pressed("primary_paint_or_drag_action")
	
	if not mouse_pressed and Input.is_action_just_pressed("undo") and undo_redo.has_undo():
		undo_redo.undo()
	if not mouse_pressed and Input.is_action_just_pressed("redo") and undo_redo.has_redo():
		undo_redo.redo()
	
	is_mouse_inside_canvas = (get_parent() as Control).get_global_rect().has_point(get_global_mouse_position())
	
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
		Enums.Mode.SELECTION_FREE:
			_handle_free_selection_mode()
	
	if Input.is_action_just_pressed("clear"):
		image.fill(Color.TRANSPARENT)
	
	if editor_texture: editor_texture.update(image)
	if preview_editor_texture: preview_editor_texture.update(preview_image)
	
	queue_redraw()

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			scale *= 1.2
		if event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			scale /= 1.2
	
		queue_redraw()

func _on_size_slider_value_changed(value: float) -> void:
	tool_brush_size[mode] = int(value)

func _on_color_picker_color_changed(color: Color) -> void:
	primary_color = color

func _on_mouse_entered() -> void:
	var hotspot := Vector2(12, 12)
	
	match mode:
		Enums.Mode.ERASER, Enums.Mode.EYEDROPPER:
			hotspot = Vector2(4, 20)
	
	Input.set_custom_mouse_cursor(cursor_icons.get(mode), Input.CURSOR_ARROW, hotspot)

func _on_mouse_exited() -> void:
	Input.set_custom_mouse_cursor(null)

func _update_mouse_state() -> bool:
	var mouse_released := Input.is_action_just_released("primary_paint_or_drag_action")
	
	# Mouse released, reset outside canvas flag
	if mouse_released and stroke_started_outside_canvas:
		stroke_started_outside_canvas = false
		return false
	
	var mouse_just_pressed := Input.is_action_just_pressed("primary_paint_or_drag_action")
	
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
	
	if Input.is_action_just_pressed("primary_paint_or_drag_action"):
		undo_redo.create_action("Paint Brush")
		undo_redo.add_undo_property(image, "data", image.data)
	if Input.is_action_just_released("primary_paint_or_drag_action"):
		undo_redo.add_do_property(image, "data", image.data)
		undo_redo.commit_action(false)
	
	if Input.is_action_pressed("primary_paint_or_drag_action"):
		points_to_paint.push_back(grid_mouse_position)
		
		if (previous_mouse_position - mouse_position).length() > 1:
			points_to_paint.append_array(Geometry2D.bresenham_line(previous_mouse_position, mouse_position))
	
	for point in points_to_paint:
		Painter.paint_pixel(image, point, brush_size, active_color)
	
	Painter.paint_pixel(preview_image, grid_mouse_position, brush_size, preview_color)

func _handle_paint_bucket_mode() -> void:
	if running_paint_bucket: return
	
	var canvas_rect := Rect2i(Vector2i.ZERO, image.get_size())
	if not canvas_rect.has_point(mouse_position): return
	
	var point_color := image.get_pixelv(mouse_position)
	
	if Input.is_action_just_pressed("primary_paint_or_drag_action"):
		if point_color == active_color:
			return
		
		undo_redo.create_action("Paint Bucket")
		undo_redo.add_undo_property(image, "data", image.data)
		
		Painter.flood_fill(image, mouse_position, active_color)
		undo_redo.add_do_property(image, "data", image.data)
		
		undo_redo.commit_action(false)

func _handle_shape_mode() -> void:
	if Input.is_action_just_pressed("primary_paint_or_drag_action"):
		drag_start_mouse_position = mouse_position
		drag_started = true
		undo_redo.create_action("Drag Drawing")
		undo_redo.add_undo_property(image, "data", image.data)
	
	if Input.is_action_just_pressed("drag_cancel"):
		drag_started = false
	
	if drag_started:
		var image_to_paint: Image
		var color_to_paint: Color
		
		if Input.is_action_pressed("primary_paint_or_drag_action"):
			image_to_paint = preview_image
			color_to_paint = preview_color
		elif Input.is_action_just_released("primary_paint_or_drag_action"):
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
	if Input.is_action_just_pressed("primary_paint_or_drag_action"):
		drag_started = true
		drag_start_mouse_position = (get_parent() as Control).get_local_mouse_position()
		initial_move_position = position
	
	if Input.is_action_just_released("primary_paint_or_drag_action"):
		drag_started = false
	
	if drag_started:
		position = initial_move_position - drag_start_mouse_position + (get_parent() as Control).get_local_mouse_position()

func _handle_eyedropper_mode() -> void:
	if Input.is_action_just_pressed("primary_paint_or_drag_action") and Painter.image_has_point(image, int(mouse_position.x), int(mouse_position.y)):
		var cursor_color := image.get_pixel(int(mouse_position.x), int(mouse_position.y))
		
		if not is_zero_approx(cursor_color.a):
			primary_color = cursor_color
			color_picker.color = primary_color

func _handle_square_selection_mode() -> void:
	if selection_area_defined: 
		if is_mouse_inside_canvas:
			if clipboard_rect.has_point(canvas.get_local_mouse_position()):
				Input.set_custom_mouse_cursor(cursor_icons.get(Enums.Mode.MOVE))
			else:
				Input.set_custom_mouse_cursor(cursor_icons.get(Enums.Mode.SELECTION_SQUARE))
		if Input.is_action_pressed("primary_paint_or_drag_action") and not drag_started:
			drag_started = true
			initial_move_position = clipboard_rect.position
			drag_start_mouse_position = canvas.get_local_mouse_position()
		if Input.is_action_just_released("primary_paint_or_drag_action"):
			drag_started = false
		if Input.is_action_just_pressed("copy"):
			clipboard_image.fill(Color.TRANSPARENT)
			clipboard_image.blit_rect(image, clipboard_rect, Vector2i.ZERO)
			%ClipboardCanvas.texture = ImageTexture.create_from_image(clipboard_image)
		if Input.is_action_just_pressed("cut"):
			clipboard_image.fill(Color.TRANSPARENT)
			clipboard_image.blit_rect(image, clipboard_rect, Vector2i.ZERO)
			%ClipboardCanvas.texture = ImageTexture.create_from_image(clipboard_image)
			undo_redo.create_action("Cut")
			undo_redo.add_undo_property(image, "data", image.data)
			image.fill_rect(clipboard_rect, Color.TRANSPARENT)
			undo_redo.add_do_property(image, "data", image.data)
			undo_redo.commit_action(false)
		if Input.is_action_just_pressed("paste"):
			undo_redo.create_action("Paste")
			undo_redo.add_undo_property(image, "data", image.data)
			image.blend_rect(clipboard_image, clipboard_image.get_used_rect(), clipboard_rect.position)
			undo_redo.add_do_property(image, "data", image.data)
			undo_redo.commit_action(false)
		if drag_started:
			var new_clipboard_rect_position := initial_move_position + canvas.get_local_mouse_position() - drag_start_mouse_position
			clipboard_rect = Rect2i(new_clipboard_rect_position, clipboard_rect.size)
	else:
		if Input.is_action_just_pressed("primary_paint_or_drag_action"):
			drag_started = true
			drag_start_mouse_position = Vector2i(mouse_position)
		
		if Input.is_action_just_released("primary_paint_or_drag_action"):
			selection_area_defined = true
			drag_started = false
			drag_end_mouse_position = Vector2i(mouse_position)
		
		var end_position := mouse_position if Input.is_action_pressed("primary_paint_or_drag_action") else drag_end_mouse_position
		_update_clipboard_rect(drag_start_mouse_position, end_position)

func _handle_free_selection_mode() -> void:
	if selection_area_defined: 
		pass
	else:
		if Input.is_action_just_pressed("primary_paint_or_drag_action"):
			drag_started = true
			selection_polygon_points.clear()
		
		if Input.is_action_just_released("primary_paint_or_drag_action"):
			selection_area_defined = true
			drag_started = false
			selection_polygon_points.push_back(Vector2i(mouse_position))
		
		if drag_started:
			selection_polygon_points.push_back(Vector2i(mouse_position))
		
		#var end_position := mouse_position if Input.is_action_pressed("primary_paint_or_drag_action") else drag_end_mouse_position
		#_update_clipboard_rect(drag_start_mouse_position, end_position)

func _update_clipboard_rect(start_position: Vector2i, end_position: Vector2i) ->void:
	var x0 := int(start_position.x)
	var y0 := int(start_position.y)
	var x1 := int(end_position.x)
	var y1 := int(end_position.y)
	if (x0 > x1):
		var temp := x0
		x0 = x1
		x1 = temp
	if (y0 > y1):
		var temp := y0
		y0 = y1
		y1 = temp
	clipboard_rect = Rect2i(Vector2i(x0, y0), Vector2i(x1, y1) - Vector2i(x0, y0))

func create_new_image(width: int, height: int) -> void:
	var new_image := Image.create_empty(width, height, false, Image.FORMAT_RGBA8)
	new_image.fill(Color.TRANSPARENT)

	set_new_image(new_image)

func set_new_image(new_image: Image) -> void:
	image = new_image
	clipboard_image = Image.create_empty(image.get_width(), image.get_height(), false, Image.FORMAT_RGBA8)
	preview_image = Image.create_empty(image.get_width(), image.get_height(), false, Image.FORMAT_RGBA8)
	
	clipboard_image.fill(Color.TRANSPARENT)
	preview_image.fill(Color.TRANSPARENT)
	
	editor_texture = ImageTexture.create_from_image(image)
	preview_editor_texture = ImageTexture.create_from_image(preview_image)

	canvas.texture = editor_texture
	preview_canvas.texture = preview_editor_texture
	
	await get_tree().process_frame
	
	offset_left = 0
	offset_top = 0
	offset_right = 0
	offset_bottom = 0
