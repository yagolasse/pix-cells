class_name Layer 
extends RefCounted

var name: StringName
var image: Image
var visible: bool = true

static func deep_duplicate_array(layers: Array[Layer]) -> Array[Layer]:
	var output: Array[Layer] = []
	
	for layer in layers:
		var new_layer := Layer.new()
		new_layer.name = layer.name
		new_layer.image = Image.create_from_data(
			layer.image.get_width(),
			layer.image.get_width(),
			layer.image.has_mipmaps(),
			layer.image.get_format(), 
			layer.image.get_data()
		)
		new_layer.visible = layer.visible
		output.push_back(new_layer)
	
	return output

func _to_string() -> String:
	return "LayerInstance: " + name
