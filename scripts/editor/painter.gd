class_name Painter
extends Node

static func paint_pixel(image: Image, position: Vector2i, brush_size: int, color: Color) -> void:
	var x := position.x
	var y := position.y
	if brush_size == 1:
		_set_pixel_safe(image, x, y, color)
	else:
		@warning_ignore("integer_division")
		var half_brush_size := brush_size / 2
		
		var point_range := range(-half_brush_size, half_brush_size + 1)
		
		for dx in point_range:
			for dy in point_range:
				_set_pixel_safe(image, x + dx, y + dy, color)

static func paint_rect(image: Image, begin: Vector2i, end: Vector2i, color: Color, fill: bool, force_square: bool) -> void:
	var x0 := begin.x
	var y0 := begin.y
	var x1 := end.x
	var y1 := end.y
	# Ensure x0,y0 is top-left and x1,y1 is bottom-right
	if x0 > x1:
		var tmp = x0
		x0 = x1
		x1 = tmp
	if y0 > y1:
		var tmp = y0
		y0 = y1
		y1 = tmp
	
	if force_square:
		@warning_ignore_start("integer_division")
		var size := mini(absi(x0 - x1), absi(y0 - y1)) / 2
		var mid_point := Vector2i((x0 + x1) / 2, (y0 + y1) / 2)
		@warning_ignore_restore("integer_division")
	
		for x in range(-size, size):
			for y in range(-size, size):
				if fill or ((x == -size or x == size - 1) or (y == -size or y == size - 1)):
					_set_pixel_safe(image, mid_point.x + x, mid_point.y + y, color)
	else:
		for x in range(x0, x1 + 1):
			for y in range(y0, y1 + 1):
				if fill or ((x == x0 or x == x1) or (y == y0 or y == y1)):
					_set_pixel_safe(image, x, y, color)

static func paint_ellipse(image: Image, begin: Vector2i, end: Vector2i, color: Color, fill: bool, force_circle: bool) -> void:
	var x0 := begin.x
	var y0 := begin.y
	var x1 := end.x
	var y1 := end.y
	# Ensure x0,y0 is top-left and x1,y1 is bottom-right
	if x0 > x1:
		var tmp = x0
		x0 = x1
		x1 = tmp
	if y0 > y1:
		var tmp = y0
		y0 = y1
		y1 = tmp

	var width = x1 - x0 + 1
	var height = y1 - y0 + 1
	if width <= 0 or height <= 0:
		return

	# High and low midpoints (differ by 1 when dimension is even)
	var midX_low = (x0 + x1) >> 1          # floor((x0+x1)/2)
	var midX_high = (x0 + x1 + 1) >> 1     # ceil((x0+x1)/2)
	var midY_low = (y0 + y1) >> 1
	var midY_high = (y0 + y1 + 1) >> 1

	# Radii (integer division, floor)
	var rx = (x1 - x0) >> 1
	var ry = (y1 - y0) >> 1

	if force_circle:
		var r = min(rx, ry)
		rx = r
		ry = r

	# Degenerate cases (line or single point)
	if rx == 0 and ry == 0:
		_set_pixel_safe(image, midX_high, midY_high, color)
		return
	if rx == 0:
		# Vertical line
		for y in range(y0, y1 + 1):
			_set_pixel_safe(image, midX_high, y, color)
		return
	if ry == 0:
		# Horizontal line
		for x in range(x0, x1 + 1):
			_set_pixel_safe(image, x, midY_high, color)
		return

	# Pre‑compute squared values
	var rx2: int = rx * rx
	var ry2: int = ry * ry
	var tworx2: int = 2 * rx2
	var twory2: int = 2 * ry2

	# Midpoint ellipse algorithm
	var x: int = 0
	var y: int = ry
	var px: int = 0
	var py: int = tworx2 * y
	@warning_ignore("integer_division")
	var p: int = ry2 - rx2 * ry + (rx2 + 2) / 4  # initial decision parameter (region 1)

	# Helper to plot / fill one point (x,y) in the first quadrant
	var process_point = func(x: int, y: int) -> void:
		if fill:
			# Fill: draw horizontal lines for top and bottom halves
			var y_top = midY_high + y
			var left = midX_low - x
			var right = midX_high + x
			for xi in range(left, right + 1):
				_set_pixel_safe(image, xi, y_top, color)

			var y_bottom = midY_low - y
			if y_bottom != y_top:  # avoid duplicate line when height is odd and y=0
				for xi in range(left, right + 1):
					_set_pixel_safe(image, xi, y_bottom, color)
		else:
			# Outline: plot the four symmetric points
			_set_pixel_safe(image, midX_high + x, midY_high + y, color)
			_set_pixel_safe(image, midX_low - x, midY_high + y, color)
			_set_pixel_safe(image, midX_high + x, midY_low - y, color)
			_set_pixel_safe(image, midX_low - x, midY_low - y, color)

	# Initial point (0, ry)
	process_point.call(x, y)

	# Region 1 (slope > -1)
	while px < py:
		x += 1
		px += twory2
		if p < 0:
			p += ry2 + px
		else:
			y -= 1
			py -= tworx2
			p += ry2 + px - py
		process_point.call(x, y)

	# Region 2 (slope <= -1)
	# Recalculate decision parameter using current x and y
	p = ry2 * (x + 1) * x + (ry2 >> 2) + rx2 * (y - 1) * (y - 1) - rx2 * ry2
	while y > 0:
		y -= 1
		py -= tworx2
		if p > 0:
			p += rx2 - py
		else:
			x += 1
			px += twory2
			p += rx2 - py + px
		process_point.call(x, y)

# Helper: Set a pixel if it lies within the image bounds.
static func _set_pixel_safe(image: Image, x: int, y: int, color: Color) -> void:
	if x >= 0 and x < image.get_width() and y >= 0 and y < image.get_height():
		image.set_pixel(x, y, color)

static func image_has_point(image: Image, x: int, y: int) -> bool:
	return x >= 0 and y >= 0 and x < image.get_width() and y < image.get_height()

## Line span flood fill algorithm.
## Taken from https://en.wikipedia.org/wiki/Flood_fill#Span_filling.
static func flood_fill(image: Image, origin: Vector2i, color: Color) -> void:
	var origin_x := origin.x
	var origin_y := origin.y
	
	var old_color := image.get_pixel(origin_x, origin_y)
	
	var point_queue: Array[PointRange] = [
		PointRange.from(origin_x, origin_x, origin_y, 1),
		PointRange.from(origin_x, origin_x, origin_y - 1, -1)
	]
	
	while not point_queue.is_empty():
		var p := point_queue.pop_back() as PointRange
		var x := p.x1
		var x1 := p.x1
		
		if image_has_point(image, x, p.y) and image.get_pixel(x, p.y) == old_color:
			while image_has_point(image, x - 1, p.y) and image.get_pixel(x - 1, p.y) == old_color:
				image.set_pixel(x - 1, p.y, color)
				x -= 1
			if x < x1:
				point_queue.push_back(PointRange.from(x, x1 - 1, p.y - p.dy, -p.dy))
		while x1 <= p.x2:
			while image_has_point(image, x1, p.y) and image.get_pixel(x1, p.y) == old_color:
				image.set_pixel(x1, p.y, color)
				x1 += 1
			if x1 > x:
				point_queue.push_back(PointRange.from(x, x1 - 1, p.y + p.dy, p.dy))
			if x1 - 1 > p.x2:
				point_queue.push_back(PointRange.from(p.x2 + 1, x1 - 1, p.y - p.dy, -p.dy))
			x1 += 1
			while x1 <= p.x2 and (not image_has_point(image, x1, p.y) or not image.get_pixel(x1, p.y) == old_color):
				x1 += 1
			x = x1

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
