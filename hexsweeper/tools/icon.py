"""Luke Steuber. Draw the original monochrome launcher mark; requires Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw

image = Image.new('1', (25, 25), 0)
draw = ImageDraw.Draw(image)
draw.ellipse((1, 1, 23, 23), outline=1, width=1)
hexagon = [(12, 6), (18, 9), (18, 15), (12, 19), (6, 15), (6, 9)]
draw.polygon(hexagon, outline=1)
for a, b in zip(hexagon, [(12, 1), (22, 6), (22, 19), (12, 23), (2, 19), (2, 6)]):
    draw.line([a, b], fill=1)
draw.rectangle((11, 9, 12, 15), fill=1)
draw.polygon([(13, 9), (16, 11), (13, 12)], fill=1)
output = Path(__file__).resolve().parent.parent / 'resources/data/menu-icon.png'
output.parent.mkdir(parents=True, exist_ok=True)
image.save(output)
