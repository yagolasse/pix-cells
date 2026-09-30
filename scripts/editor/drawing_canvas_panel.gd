class_name DrawingCanvasPanel
extends Panel

@onready var size_slider: SpinBox = %SizeSlider
@onready var zoom_slider: SpinBox = %ZoomSlider
@onready var image_editor: Control = %ImageEditor

func _ready() -> void:
	zoom_slider.value_changed.connect(_on_zoom_slider_value_changed)

func _gui_input(event: InputEvent) -> void:
	if size_slider.visible:
		if event.is_action_pressed(&"brush_increase_size"): size_slider.value += 1
		elif event.is_action_pressed(&"brush_decrease_size"): size_slider.value -= 1
	
	if event.is_action_pressed(&"zoom_in", false, true): 
		image_editor.scale *= 1.2
		zoom_slider.value = image_editor.scale.x * 100
	elif event.is_action_pressed(&"zoom_out", false, true): 
		image_editor.scale /= 1.2
		zoom_slider.value = image_editor.scale.x * 100
	
	if event is InputEventMagnifyGesture:
		var factor := (event as InputEventMagnifyGesture).factor
		if event.get_modifiers_mask() == KeyModifierMask.KEY_MASK_CMD_OR_CTRL:
			size_slider.value *= factor
		else:
			image_editor.scale *= factor 
			zoom_slider.value = image_editor.scale.x * 100

func _on_zoom_slider_value_changed(value: float) -> void:
	image_editor.scale = Vector2.ONE * value / 100.0
