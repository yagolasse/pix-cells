class_name DrawingCanvasPanel
extends Panel

@onready var size_slider: SpinBox = %SizeSlider
@onready var image_editor: Control = %ImageEditor

func _gui_input(event: InputEvent) -> void:
	if size_slider.visible:
		if event.is_action_pressed(&"brush_increase_size"): size_slider.value += 1
		elif event.is_action_pressed(&"brush_decrease_size"): size_slider.value -= 1
	
	if event.is_action_pressed(&"zoom_in", false, true): image_editor.scale *= 1.2
	elif event.is_action_pressed(&"zoom_out", false, true): image_editor.scale /= 1.2
