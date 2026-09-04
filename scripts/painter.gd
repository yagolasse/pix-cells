class_name Painter
extends Node

static func image_has_point(image: Image, x: int, y: int) -> bool:
    return x > 0 and y > 0 and x < image.get_width() and y < image.get_height()

static func paint_pixel(image: Image, x: int, y: int, brush_size: int, color: Color) -> void:
    if brush_size == 1 and image_has_point(image, x, y):
        image.set_pixel(x, y, color)
    else:
        @warning_ignore("integer_division")
        var half_brush_size := brush_size / 2

        var offset := 0 if brush_size % 2 == 0 else 1
        var point_range := range(-half_brush_size, half_brush_size + offset)

        for dx in point_range:
            for dy in point_range:
                if not image_has_point(image, x, y): continue
                
                image.set_pixel(x + dx, y + dy, color)
