"""Dateisymbole für Videos und Musik, die mit Vidline geöffnet werden: Blatt mit Eselsohr und Zeilen, das Vidline-Logo
in der Mitte, darunter die Endung (mp3, mp4 ...) und ganz unten ein Streifen - je Typ in eigener Farbe, hell und dunkel.
Welche Variante Windows zeigt, stellt man in Vidline ein (src/fileicons.cpp).
Schreibt assets/filetypes/light|dark/<endung>.ico und packaging/windows/filetypes.iss (Registry-Einträge für den Installer).
Braucht Python 3 + Pillow und Segoe UI (Windows). Aufruf: python tools_make_filetype_icons.py"""
import io
import os
import struct

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, 'assets', 'filetypes')
LOGO = Image.open(os.path.join(ROOT, 'assets', 'icon.png')).convert('RGBA')
FONT = next((f for f in ['C:/Windows/Fonts/segoeuib.ttf', 'C:/Windows/Fonts/arialbd.ttf'] if os.path.exists(f)), None)

# Endung -> (Beschriftung, Farbe, Art)
TYPES = {
    'mp4': ('mp4', '#1d4ed8', 'video'), 'mov': ('mov', '#0f766e', 'video'), 'mkv': ('mkv', '#7e22ce', 'video'),
    'm4v': ('m4v', '#1e40af', 'video'), 'avi': ('avi', '#b91c1c', 'video'), 'webm': ('webm', '#15803d', 'video'),
    'wmv': ('wmv', '#0369a1', 'video'), 'flv': ('flv', '#c2410c', 'video'),
    'mp3': ('mp3', '#7c3aed', 'audio'), 'wav': ('wav', '#0891b2', 'audio'), 'm4a': ('m4a', '#db2777', 'audio'),
    'aac': ('aac', '#a21caf', 'audio'), 'flac': ('flac', '#059669', 'audio'), 'ogg': ('ogg', '#d97706', 'audio'),
    'opus': ('opus', '#4f46e5', 'audio'), 'wma': ('wma', '#2563eb', 'audio'),
}

LIGHT = {'paper': (255, 255, 255), 'outline': (176, 182, 194), 'fold': (226, 230, 237), 'lines': (229, 232, 238), 'shadow': 60}
DARK = {'paper': (43, 45, 50), 'outline': (88, 92, 102), 'fold': (62, 65, 72), 'lines': (60, 63, 70), 'shadow': 110}


def hexrgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def page(size, label, color, detail, dark=False):
    """Ein Symbol in 'size' Pixeln, gezeichnet in 4-facher Größe und verkleinert (glatte Kanten)."""
    P = DARK if dark else LIGHT
    col = hexrgb(color)
    if dark:  # auf dunklem Blatt aufhellen, damit die Endung lesbar bleibt
        col = tuple(round(c + (255 - c) * 0.38) for c in col)
    S = size * 4
    u = S / 256
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    left, top, right, bottom, fold = 40 * u, 10 * u, 216 * u, 246 * u, 52 * u
    if size <= 24:
        left, right, top, bottom, fold = 34 * u, 222 * u, 6 * u, 250 * u, 60 * u
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).polygon([(left, top + 4 * u), (right - fold, top + 4 * u), (right, top + fold + 4 * u), (right, bottom + 4 * u), (left, bottom + 4 * u)], fill=(0, 0, 0, P['shadow']))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(5 * u)))
    outline = P['outline'] + (255,)
    w = max(1, round(3 * u)) if size >= 32 else max(1, round(5 * u))
    d.polygon([(left, top), (right - fold, top), (right, top + fold), (right, bottom), (left, bottom)], fill=P['paper'] + (255,), outline=outline, width=w)
    d.polygon([(right - fold, top), (right - fold, top + fold), (right, top + fold)], fill=P['fold'] + (255,), outline=outline, width=w)
    if detail:
        y = top + 34 * u
        while y < bottom - 34 * u:
            x2 = right - (fold + 14 * u if y < top + fold + 6 * u else 22 * u)
            d.rounded_rectangle([left + 20 * u, y, x2, y + 7 * u], radius=3.5 * u, fill=P['lines'] + (255,))
            y += 19 * u
    ls = (104 if detail else 150) * u
    cx = (left + right) / 2
    cy = (top + 98 * u) if detail else (top + bottom) / 2
    if detail:
        d.rounded_rectangle([cx - ls / 2 - 7 * u, cy - ls / 2 - 7 * u, cx + ls / 2 + 7 * u, cy + ls / 2 + 7 * u], radius=30 * u, fill=P['paper'] + (255,))
    img.alpha_composite(LOGO.resize((round(ls), round(ls)), Image.LANCZOS), (round(cx - ls / 2), round(cy - ls / 2)))
    if detail and label:
        fs = 58 * u if len(label) <= 3 else (48 * u if len(label) == 4 else 40 * u)
        font = ImageFont.truetype(FONT, round(fs))
        tw = d.textlength(label, font=font)
        ty = bottom - 84 * u
        x0, y0, x1, y1 = d.textbbox((cx - tw / 2, ty - 8 * u), label, font=font)
        d.rectangle([x0 - 8 * u, y0 - 6 * u, x1 + 8 * u, y1 + 6 * u], fill=P['paper'] + (255,))
        d.text((cx - tw / 2, ty - 8 * u), label, font=font, fill=col + (255,))
    if label and size >= 32:  # farbiger Streifen ganz unten
        d.rectangle([left + w, bottom - 16 * u, right - w + 1, bottom], fill=col + (255,))
    return img.resize((size, size), Image.LANCZOS)


# ---------------------------------------------------------------- kleine Größen (16-40 px)
# Logo und Endung sind dort zu klein. Stattdessen: Blatt, darauf unten ein breites farbiges Schild mit der Endung in
# weißen Großbuchstaben (16/20 px als Pixelschrift, ab 24 px Segoe UI), ab 20 px oben klein das Logo.
SMALL_LIGHT = {'paper': (255, 255, 255), 'outline': (150, 156, 168), 'fold': (226, 230, 237), 'lines': (205, 209, 216)}
SMALL_DARK = {'paper': (43, 45, 50), 'outline': (110, 114, 124), 'fold': (62, 65, 72), 'lines': (78, 81, 90)}
GLYPHS = {  # 3x5 Pixel je Zeichen
    'A': '.#.|#.#|###|#.#|#.#', 'B': '##.|#.#|##.|#.#|##.', 'C': '.##|#..|#..|#..|.##', 'D': '##.|#.#|#.#|#.#|##.',
    'E': '###|#..|##.|#..|###', 'F': '###|#..|##.|#..|#..', 'G': '.##|#..|#.#|#.#|.##', 'H': '#.#|#.#|###|#.#|#.#',
    'I': '###|.#.|.#.|.#.|###', 'J': '..#|..#|..#|#.#|.#.', 'K': '#.#|#.#|##.|#.#|#.#', 'L': '#..|#..|#..|#..|###',
    'M': '#.#|###|###|#.#|#.#', 'N': '##.|#.#|#.#|#.#|#.#', 'O': '.#.|#.#|#.#|#.#|.#.', 'P': '##.|#.#|##.|#..|#..',
    'Q': '.#.|#.#|#.#|##.|.##', 'R': '##.|#.#|##.|#.#|#.#', 'S': '.##|#..|.#.|..#|##.', 'T': '###|.#.|.#.|.#.|.#.',
    'U': '#.#|#.#|#.#|#.#|###', 'V': '#.#|#.#|#.#|#.#|.#.', 'W': '#.#|#.#|###|###|#.#', 'X': '#.#|#.#|.#.|#.#|#.#',
    'Y': '#.#|#.#|.#.|.#.|.#.', 'Z': '###|..#|.#.|#..|###',
    '0': '###|#.#|#.#|#.#|###', '1': '.#.|##.|.#.|.#.|###', '2': '##.|..#|.#.|#..|###', '3': '##.|..#|.#.|..#|##.',
    '4': '#.#|#.#|###|..#|..#', '5': '###|#..|##.|..#|##.', '6': '.##|#..|###|#.#|###', '7': '###|..#|.#.|.#.|.#.',
    '8': '###|#.#|###|#.#|###', '9': '###|#.#|###|..#|##.',
    '+': '...|.#.|###|.#.|...', '#': '#.#|###|#.#|###|#.#', '<': '..#|.#.|#..|.#.|..#', '>': '#..|.#.|..#|.#.|#..',
    '/': '..#|..#|.#.|#..|#..',
}


def small(size, label, color, dark=False):
    P = SMALL_DARK if dark else SMALL_LIGHT
    col = hexrgb(color)
    s, K = size, 4
    left = int(s * 0.125 + 0.5)
    right, fold = s - left, int(s * 0.3 + 0.5)
    # Blatt mit 1 px Rand und Eselsohr, auf ganze Pixel ausgerichtet (4-fach gezeichnet, dann verkleinert)
    img = Image.new('RGBA', (s * K, s * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    poly = lambda pts, fill: d.polygon([(x * K, y * K) for x, y in pts], fill=fill + (255,))
    poly([(left, 0), (right - fold, 0), (right, fold), (right, s), (left, s)], P['outline'])
    poly([(left + 1, 1), (right - fold - 0.4, 1), (right - 1, fold + 0.4), (right - 1, s - 1), (left + 1, s - 1)], P['paper'])
    poly([(right - fold, 0), (right - fold, fold), (right, fold)], P['outline'])
    poly([(right - fold + 1, 1.25), (right - fold + 1, fold - 1), (right - 1.25, fold - 1)], P['fold'])
    img = img.resize((s, s), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    text, font = label.upper(), None
    if s >= 24:  # größte Segoe-Größe, die aufs Schild passt - sonst Pixelschrift
        for fs in range(round(s * 0.42), 6, -1):
            f = ImageFont.truetype(FONT, fs)
            x0, _, x1, _ = d.textbbox((0, 0), text, font=f)
            if x1 - x0 <= s - 2:
                font = f
                break
    if font is None:
        if len(text) * 4 - 1 > s:  # 16 px: höchstens 4 Zeichen
            text = text[:3]
        tw, th = len(text) * 4 - 1, 5
    else:
        hb = d.textbbox((0, 0), 'H', font=font)
        tw, th = x1 - x0, hb[3] - hb[1]  # Breite, Versalhöhe
    pad = 1 if s < 20 else 2 if s < 32 else 3
    bh = th + 2 * pad
    by = s - 1 - bh
    bw = min(s, max(tw + 2 * pad, right - left + 2))
    bx = (s - bw) // 2
    d.rectangle([bx, by, bx + bw - 1, by + bh - 1], fill=col + (255,))
    for cx, cy in ((bx, by), (bx + bw - 1, by), (bx, by + bh - 1), (bx + bw - 1, by + bh - 1)):
        img.putpixel((cx, cy), col + (110,))  # Ecken leicht abgerundet
    tx = bx + (bw - tw) // 2
    if font is None:
        for ch in text:
            for r, row in enumerate(GLYPHS.get(ch, '...|...|...|...|...').split('|')):
                for c, v in enumerate(row):
                    if v == '#':
                        img.putpixel((tx + c, by + pad + r), (255, 255, 255, 255))
            tx += 4
    else:
        d.text((tx - x0, by + pad - hb[1]), text, font=font, fill=(255, 255, 255, 255))
    if s >= 20:  # Logo oben links auf dem Blatt
        ls = min(by - 3, right - left - fold)
        img.alpha_composite(LOGO.resize((ls, ls), Image.LANCZOS), (left + 2, 2))
    else:  # 16 px: zwei Zeilen statt Logo
        for y in range(3, by - 1, 2):
            d.line([(left + 2, y), (right - (fold + 1 if y < fold + 1 else 3), y)], fill=P['lines'] + (255,))
    return img


SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def icon(size, label, color, dark=False):
    return small(size, label, color, dark) if size < 48 else page(size, label, color, True, dark)


def png(img):
    b = io.BytesIO()
    img.save(b, 'PNG', optimize=True)
    return b.getvalue()


def write_ico(path, images):
    head = struct.pack('<HHH', 0, 1, len(images))
    entries, data = b'', b''
    offset = 6 + 16 * len(images)
    for size, blob in images:
        entries += struct.pack('<BBBBHHII', 0 if size >= 256 else size, 0 if size >= 256 else size, 0, 0, 1, 32, len(blob), offset)
        data += blob
        offset += len(blob)
    open(path, 'wb').write(head + entries + data)


icons = {ext: (label, color) for ext, (label, color, _) in TYPES.items()}
for variant in ('light', 'dark'):
    os.makedirs(os.path.join(OUT, variant), exist_ok=True)
    for f in os.listdir(os.path.join(OUT, variant)):  # z. B. video.ico/audio.ico älterer Versionen nicht mehr mitliefern
        if os.path.splitext(f)[0] not in icons:
            os.remove(os.path.join(OUT, variant, f))
    for name, (label, color) in icons.items():
        write_ico(os.path.join(OUT, variant, f'{name}.ico'),
                  [(s, png(icon(s, label, color, variant == 'dark'))) for s in SIZES])
print(f'{len(icons)} Dateisymbole (hell + dunkel) nach {OUT}')

# ---------------------------------------------------------------- Installer: Registry-Einträge
L = ['; Erzeugt von tools_make_filetype_icons.py - nicht von Hand ändern. Wird in vidline.iss eingebunden.',
     '; Je Dateityp eine ProgID (Cutline.mp3 ...) mit eigenem Dateisymbol. Unter "Öffnen mit" steht Vidline nur über diese',
     '; ProgIDs - ein zusätzlicher Eintrag über Applications\\Vidline.exe würde für alle Typen das Programmsymbol zeigen.']
for ext, (label, color, kind) in TYPES.items():
    L += [
        f'Root: HKA; Subkey: "Software\\Classes\\Cutline.{ext}"; ValueType: string; ValueData: "{label.upper()} ({kind})"; Flags: uninsdeletekey; Tasks: openwith',
        f'Root: HKA; Subkey: "Software\\Classes\\Cutline.{ext}\\DefaultIcon"; ValueType: string; ValueData: "{{app}}\\filetypes\\light\\{ext}.ico"; Tasks: openwith',
        f'Root: HKA; Subkey: "Software\\Classes\\Cutline.{ext}\\shell\\open\\command"; ValueType: string; ValueData: """{{app}}\\Vidline.exe"" ""%1"""; Tasks: openwith',
        f'Root: HKA; Subkey: "Software\\Classes\\.{ext}\\OpenWithProgids"; ValueType: string; ValueName: "Cutline.{ext}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith',
        f'Root: HKA; Subkey: "Software\\Vidline\\Capabilities\\FileAssociations"; ValueType: string; ValueName: ".{ext}"; ValueData: "Cutline.{ext}"; Tasks: openwith',
        # aufräumen: Einträge älterer Versionen (gemeinsame ProgID, Applications-Liste)
        f'Root: HKA; Subkey: "Software\\Classes\\.{ext}\\OpenWithList\\Cutline.exe"; ValueType: none; Flags: deletekey',
        f'Root: HKA; Subkey: "Software\\Classes\\.{ext}\\OpenWithProgids"; ValueType: none; ValueName: "Cutline.{"Video" if kind == "video" else "Audio"}"; Flags: deletevalue',
        # Vidline kann das Symbol (hell/dunkel) für den Benutzer umgestellt haben: beim Deinstallieren mit entfernen
        f'Root: HKCU; Subkey: "Software\\Classes\\Cutline.{ext}"; Flags: uninsdeletekey dontcreatekey',
        # Symbol je Endung: greift, wenn der gewählte Eintrag selbst keins hat (z. B. Vidline.Video älterer Versionen -
        # sonst stünde bei jedem Video "VIDEO" statt "MP4"). Übergeordneten Schlüssel zuerst, damit er beim Deinstallieren
        # (umgekehrte Reihenfolge) erst nach DefaultIcon geprüft und nur gelöscht wird, wenn er leer ist.
        f'Root: HKA; Subkey: "Software\\Classes\\SystemFileAssociations\\.{ext}"; Flags: uninsdeletekeyifempty; Tasks: openwith',
        f'Root: HKA; Subkey: "Software\\Classes\\SystemFileAssociations\\.{ext}\\DefaultIcon"; ValueType: string; ValueData: "{{app}}\\filetypes\\light\\{ext}.ico"; Flags: uninsdeletekey; Tasks: openwith',
        f'Root: HKCU; Subkey: "Software\\Classes\\SystemFileAssociations\\.{ext}"; Flags: uninsdeletekeyifempty dontcreatekey',
        f'Root: HKCU; Subkey: "Software\\Classes\\SystemFileAssociations\\.{ext}\\DefaultIcon"; Flags: uninsdeletekey dontcreatekey',
        # Symbol, das Vidline dem von Windows angelegten "<endung>_auto_file" gegeben hat (src/fileicons.cpp)
        f'Root: HKCU; Subkey: "Software\\Classes\\{ext}_auto_file\\DefaultIcon"; Flags: uninsdeletekey dontcreatekey',
    ]
L += [
    'Root: HKA; Subkey: "Software\\Classes\\Applications\\Cutline.exe\\SupportedTypes"; ValueType: none; Flags: deletekey',
    '; Sammel-ProgIDs älterer Versionen bleiben (falls Windows sie als Standard gespeichert hat), ohne eigenes Symbol',
]
for kind in ('Video', 'Audio'):
    L += [
        f'Root: HKA; Subkey: "Software\\Classes\\Cutline.{kind}"; ValueType: string; ValueData: "{kind}"; Flags: uninsdeletekey; Tasks: openwith',
        # ohne eigenes Symbol, damit das Symbol der Endung (MP4, MP3 ...) greift
        f'Root: HKA; Subkey: "Software\\Classes\\Cutline.{kind}\\DefaultIcon"; ValueType: none; Flags: deletekey; Tasks: openwith',
        f'Root: HKCU; Subkey: "Software\\Classes\\Cutline.{kind}\\DefaultIcon"; ValueType: none; Flags: deletekey',
        f'Root: HKA; Subkey: "Software\\Classes\\Cutline.{kind}\\shell\\open\\command"; ValueType: string; ValueData: """{{app}}\\Vidline.exe"" ""%1"""; Tasks: openwith',
        f'Root: HKCU; Subkey: "Software\\Classes\\Cutline.{kind}"; Flags: uninsdeletekey dontcreatekey',
    ]
open(os.path.join(ROOT, 'packaging', 'windows', 'filetypes.iss'), 'w', encoding='utf-8-sig', newline='\r\n').write('\n'.join(L) + '\n')
print('packaging/windows/filetypes.iss geschrieben')
