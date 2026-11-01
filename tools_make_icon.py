"""Erzeugt assets/icon.ico (mehrere Größen) mit dem gleichen Design wie makeAppPixmap() in icons.cpp."""
import struct, sys
from PySide6.QtCore import Qt, QRectF, QBuffer, QIODevice, QByteArray
from PySide6.QtGui import QGuiApplication, QImage, QPainter, QLinearGradient, QColor, QPainterPath, QPen

app = QGuiApplication(sys.argv)

def draw(size):
    im = QImage(size, size, QImage.Format_ARGB32)
    im.fill(Qt.transparent)
    p = QPainter(im)
    p.setRenderHint(QPainter.Antialiasing)
    p.scale(size / 100.0, size / 100.0)
    g = QLinearGradient(0, 0, 100, 100)
    g.setColorAt(0, QColor("#22d3a0")); g.setColorAt(1, QColor("#2563eb"))
    p.setPen(Qt.NoPen); p.setBrush(g)
    p.drawRoundedRect(QRectF(4, 4, 92, 92), 22, 22)
    p.setBrush(QColor(255, 255, 255, 235))
    t = QPainterPath(); t.moveTo(38, 27); t.lineTo(74, 50); t.lineTo(38, 73); t.closeSubpath()
    p.drawPath(t)
    p.setPen(QPen(QColor(255, 255, 255, 200), 5, Qt.SolidLine, Qt.RoundCap))
    p.drawLine(24, 20, 24, 80)
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
open("assets/icon.ico", "wb").write(out)
draw(256).save("assets/icon.png")

# macOS .icns (PNG-Chunks): ic07=128, ic08=256, ic09=512, ic10=1024
chunks = b""
for tag, sz in ((b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024)):
    ba = QByteArray(); buf = QBuffer(ba); buf.open(QIODevice.WriteOnly)
    draw(sz).save(buf, "PNG")
    d = bytes(ba)
    chunks += tag + struct.pack(">I", len(d) + 8) + d
open("assets/icon.icns", "wb").write(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)
print("ok", len(out))
