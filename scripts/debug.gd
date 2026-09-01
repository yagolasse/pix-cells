class_name Debug
extends Control

static var instance: Debug

@export var container: VBoxContainer

var properties: Array[StringName]
var fps_ms: int

func _ready() -> void:
	instance = self
	visible = false

func _process(delta: float) -> void:
	fps_ms = int(1 / delta)

func _input(event: InputEvent) -> void:
	if event.is_action_pressed("debug"):
		visible = not visible
		get_viewport().set_input_as_handled()

func add_debug_property(id: StringName, value: Variant, time_in_frames: int = 1) -> void:
	if properties.has(id):
		@warning_ignore("integer_division")
		if Time.get_ticks_msec() / fps_ms % time_in_frames == 0:
			var target := container.find_child(id, true, false) as Label
			target.text = "%s: %s" % [id, value]
	else:
		var property := Label.new()
		property.name = id
		property.text = "%s: %s" % [id, value]

		container.add_child(property)

		properties.push_back(id)
