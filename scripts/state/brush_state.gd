class_name BrushState
extends State

@onready var size_slider: SpinBox = %SizeSlider
@onready var image_editor: ImageEditor = %ImageEditor

func enter(_previous_state_path: String, _data := {}) -> void:
	size_slider.visible = true

func update(_delta: float) -> void:
	var points_to_paint: Array[Vector2i]
	var grid_mouse_position := Vector2i(image_editor.mouse_position)
	
	if Input.is_action_just_pressed("primary_paint_or_drag_action"):
		image_editor.undo_redo.create_action("Paint Brush")
		image_editor.undo_redo.add_undo_property(image_editor.image, "data", image_editor.image.data)
	if Input.is_action_just_released("primary_paint_or_drag_action"):
		image_editor.undo_redo.add_do_property(image_editor.image, "data", image_editor.image.data)
		image_editor.undo_redo.commit_action(false)
	
	if Input.is_action_pressed("primary_paint_or_drag_action"):
		points_to_paint.push_back(grid_mouse_position)
		
		if (image_editor.previous_mouse_position - image_editor.mouse_position).length() > 1:
			points_to_paint.append_array(Geometry2D.bresenham_line(image_editor.previous_mouse_position, image_editor.mouse_position))
	
	for point in points_to_paint:
		Painter.paint_pixel(image_editor.image, point, image_editor.brush_size, image_editor.active_color)
	
	Painter.paint_pixel(image_editor.preview_image, grid_mouse_position, image_editor.brush_size, image_editor.preview_color)

func exit() -> void:
	size_slider.visible = false
