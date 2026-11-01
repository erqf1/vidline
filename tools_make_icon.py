"""Erzeugt das Vidline-Logo (assets/icon.ico, icon.png, icon.icns): verschwommene Farben hinter Milchglas (Acryl),
darauf ein weißes Play-Symbol. Das Programm zeigt dasselbe Bild (makeAppPixmap() in icons.cpp lädt icon.png)."""
import struct
from io import BytesIO
from PIL import Image, ImageChops, ImageDraw, ImageFilter

S = 1024


def rounded(size, box, radius):
    m = Image.new('L', (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius, fill=255)
    return m


def master():
    # Farbflecken, stark verwischt: wie ein Video, das man durch Milchglas sieht
    im = Image.new('RGB', (S, S), (40, 37, 44))
    d = ImageDraw.Draw(im)
    for box, col in (((560, -120, 1160, 520), (247, 124, 46)), ((-160, -140, 480, 420), (118, 72, 214)),
                     ((-140, 560, 560, 1180), (219, 52, 96)), ((520, 600, 1180, 1200), (24, 182, 164)),
                     ((300, 300, 720, 720), (96, 46, 40))):
        d.ellipse(box, fill=col)
    im = im.filter(ImageFilter.GaussianBlur(170))
    # Tönung wie beim Acryl: alles etwas dunkler und ruhiger
    im = Image.blend(im, Image.new('RGB', (S, S), (30, 28, 34)), .26)

    out = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    box = (41, 41, 983, 983)
    mask = rounded(S, box, 214)
    out.paste(im, (0, 0), mask)

    # Licht von oben und helle Glaskante
    sheen = Image.new('L', (S, S), 0)
    for y in range(41, 520):
        ImageDraw.Draw(sheen).line([(0, y), (S, y)], fill=int(46 * (1 - (y - 41) / 479) ** 2))
    sheen = ImageChops.multiply(sheen, mask)
    out.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), sheen)
    rim = ImageChops.subtract(mask, rounded(S, (51, 51, 973, 973), 204))
    rim_top = Image.new('L', (S, S), 0)
    for y in range(S):
        ImageDraw.Draw(rim_top).line([(0, y), (S, y)], fill=int(120 - 84 * y / S))
    out.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), ImageChops.multiply(rim, rim_top))

    # Play-Symbol mit runden Ecken und weichem Schatten
    tri = Image.new('L', (S * 2, S * 2), 0)
    pts = [(736, 560), (736, 1488), (1500, 1024)]
    d = ImageDraw.Draw(tri)
    d.polygon(pts, fill=255)
    d.line(pts + pts[:1], fill=255, width=96, joint='curve')
    for x, y in pts:
        d.ellipse((x - 48, y - 48, x + 48, y + 48), fill=255)
    tri = tri.resize((S, S), Image.LANCZOS)
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    shadow.paste(Image.new('RGBA', (S, S), (10, 8, 14, 150)), (0, 26), tri)
    out.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(30)))
    out.paste(Image.new('RGBA', (S, S), (255, 255, 255, 245)), (0, 0), tri)
    return out


M = master()


def at(size):
    return M.resize((size, size), Image.LANCZOS)


def png(size):
    b = BytesIO()
    at(size).save(b, 'PNG')
    return b.getvalue()


sizes = [16, 24, 32, 48, 64, 128, 256]
pngs = [png(s) for s in sizes]
out = struct.pack('<HHH', 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
for s, d in zip(sizes, pngs):
    out += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(d), off)
    off += len(d)
out += b''.join(pngs)
open('assets/icon.ico', 'wb').write(out)
at(256).save('assets/icon.png')

# macOS .icns (PNG-Chunks): ic07=128, ic08=256, ic09=512, ic10=1024
chunks = b''
for tag, sz in ((b'ic07', 128), (b'ic08', 256), (b'ic09', 512), (b'ic10', 1024)):
    d = png(sz)
    chunks += tag + struct.pack('>I', len(d) + 8) + d
open('assets/icon.icns', 'wb').write(b'icns' + struct.pack('>I', len(chunks) + 8) + chunks)
print('ok', len(out))
