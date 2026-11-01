"""Erzeugt assets/file.ico: Symbol für Videos und Musik, die mit Vidline geöffnet werden - weißes Blatt mit
umgeknickter Ecke, das Vidline-Logo (wie makeAppPixmap() in icons.cpp) in der Mitte."""
import struct, sys
from PySide6.QtCore import Qt, QRectF, QPointF, QBuffer, QIODevice, QByteArray
from PySide6.QtGui import QGuiApplication, QImage, QPainter, QLinearGradient, QColor, QPainterPath, QPen

app = QGuiApplication(sys.argv)


def logo(p, x, y, s):
    """Vidline-Logo im Quadrat (x, y, s)"""
    p.setRenderHint(QPainter.SmoothPixmapTransform)
    p.drawImage(QRectF(x, y, s, s), QImage("assets/icon.png"))


def draw(size):
    im = QImage(size, size, QImage.Format_ARGB32)
    im.fill(Qt.transparent)
    p = QPainter(im)
    p.setRenderHint(QPainter.Antialiasing)
    p.scale(size / 100.0, size / 100.0)
    # Blatt (Hochformat) mit umgeknickter Ecke oben rechts
    l, t, r, b, fold = 17, 6, 83, 94, 18
    page = QPainterPath()
    page.moveTo(l + 3, t); page.lineTo(r - fold, t); page.lineTo(r, t + fold); page.lineTo(r, b - 3)
    page.quadTo(r, b, r - 3, b); page.lineTo(l + 3, b); page.quadTo(l, b, l, b - 3); page.lineTo(l, t + 3)
    page.quadTo(l, t, l + 3, t); page.closeSubpath()
    p.setPen(QPen(QColor(0, 0, 0, 70), 1.6 if size >= 32 else 3))
    p.setBrush(QColor("#ffffff"))
    p.drawPath(page)
    corner = QPainterPath()
    corner.moveTo(r - fold, t); corner.lineTo(r - fold, t + fold - 3); corner.quadTo(r - fold, t + fold, r - fold + 3, t + fold)
    corner.lineTo(r, t + fold); corner.closeSubpath()
    p.setBrush(QColor("#dde3ea"))
    p.drawPath(corner)
    # Logo in der Mitte (bei kleinen Größen größer, damit man es noch erkennt)
    s = 56 if size >= 48 else 64
    logo(p, 50 - s / 2, 54 - s / 2, s)
    p.end()
    return im


sizes = [16, 24, 32, 48, 64, 128, 256]
pngs = []
for s in sizes:
    ba = QByteArray(); buf = QBuffer(ba); buf.open(QIODevice.WriteOnly)
    draw(s).save(buf, "PNG"); pngs.append(bytes(ba))
out = struct.pack("<HHH", 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
for s, d in zip(sizes, pngs):
    out += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(d), off)
    off += len(d)
out += b"".join(pngs)
open("assets/file.ico", "wb").write(out)
print("ok", len(out))
