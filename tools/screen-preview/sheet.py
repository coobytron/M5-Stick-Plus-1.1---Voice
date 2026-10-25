"""Turns the preview's .pgm frames into PNGs, plus one contact sheet at 3x."""
import pathlib
import sys

from PIL import Image, ImageDraw

out = pathlib.Path(sys.argv[1])
frames = sorted(out.glob("*.pgm"))
scale, pad, label_h = 3, 16, 18
images = []
for f in frames:
    im = Image.open(f).convert("L")
    im.save(f.with_suffix(".png"))
    images.append((f.stem, im.resize((im.width * scale, im.height * scale), Image.NEAREST)))
w = sum(im.width for _, im in images) + pad * (len(images) + 1)
h = max(im.height for _, im in images) + 2 * pad + label_h
sheet = Image.new("L", (w, h), 200)
draw = ImageDraw.Draw(sheet)
x = pad
for name, im in images:
    sheet.paste(im, (x, pad))
    draw.rectangle([x - 1, pad - 1, x + im.width, pad + im.height], outline=120)
    draw.text((x, pad + im.height + 4), name, fill=0)
    x += im.width + pad
sheet.save(out / "sheet.png")
print(f"{len(images)} screens -> {out / 'sheet.png'}")
