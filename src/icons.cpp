#include "icons.h"

#include <cmath>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

QIcon makeIcon(Ic id, const QColor& c) {
    const int S = 96;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(S / 100.0, S / 100.0);
    QPen pen(c, 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    auto fill = [&] { p.setPen(Qt::NoPen); p.setBrush(c); };
    auto stroke = [&] { p.setPen(pen); p.setBrush(Qt::NoBrush); };

    switch (id) {
    case Ic::Open: {
        QPainterPath f;
        f.moveTo(10, 30); f.lineTo(10, 78); f.quadTo(10, 84, 16, 84); f.lineTo(84, 84); f.quadTo(90, 84, 90, 78);
        f.lineTo(90, 38); f.quadTo(90, 32, 84, 32); f.lineTo(46, 32); f.lineTo(38, 20); f.lineTo(16, 20);
        f.quadTo(10, 20, 10, 26); f.closeSubpath();
        p.drawPath(f);
        p.drawLine(10, 46, 90, 46);
        break;
    }
    case Ic::AddVideo: {
        p.drawRoundedRect(QRectF(8, 26, 58, 48), 9, 9);
        QPainterPath t; t.moveTo(72, 42); t.lineTo(92, 30); t.lineTo(92, 70); t.lineTo(72, 58); t.closeSubpath();
        p.drawPath(t);
        break;
    }
    case Ic::AddAudio:
    case Ic::Music: {
        fill();
        p.drawEllipse(QPointF(30, 76), 12, 10);
        p.drawEllipse(QPointF(70, 68), 12, 10);
        stroke();
        p.drawLine(41, 74, 41, 24);
        p.drawLine(81, 66, 81, 18);
        p.drawLine(41, 24, 81, 18);
        break;
    }
    case Ic::ChevronDown:
    case Ic::ChevronUp: {
        // Pfeil + Strich: "Leiste nach unten wegschieben" bzw. "wieder hochholen"
        const bool down = id == Ic::ChevronDown;
        QPainterPath a;
        a.moveTo(24, down ? 34 : 58);
        a.lineTo(50, down ? 58 : 34);
        a.lineTo(76, down ? 34 : 58);
        p.drawPath(a);
        p.drawLine(24, down ? 76 : 76, 76, 76);
        break;
    }
    case Ic::ZoomIn:
    case Ic::ZoomOut: {
        p.drawEllipse(QPointF(42, 42), 26, 26);
        p.drawLine(62, 62, 88, 88);
        p.drawLine(30, 42, 54, 42);
        if (id == Ic::ZoomIn) p.drawLine(42, 30, 42, 54);
        break;
    }
    case Ic::Text: {
        p.drawLine(18, 22, 82, 22);
        p.drawLine(18, 22, 18, 32);
        p.drawLine(82, 22, 82, 32);
        p.drawLine(50, 22, 50, 82);
        p.drawLine(38, 82, 62, 82);
        break;
    }
    case Ic::Edit: {
        p.drawLine(24, 76, 34, 52);
        p.drawLine(34, 52, 68, 18);
        p.drawLine(68, 18, 82, 32);
        p.drawLine(82, 32, 48, 66);
        p.drawLine(48, 66, 24, 76);
        p.drawLine(58, 28, 72, 42);
        break;
    }
    case Ic::Export: {
        p.drawLine(50, 62, 50, 12);
        p.drawLine(50, 12, 32, 30);
        p.drawLine(50, 12, 68, 30);
        p.drawLine(16, 56, 16, 84);
        p.drawLine(16, 84, 84, 84);
        p.drawLine(84, 84, 84, 56);
        break;
    }
    case Ic::Settings: {
        p.drawEllipse(QPointF(50, 50), 27, 27);
        p.drawEllipse(QPointF(50, 50), 10, 10);
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(50, 50);
            p.rotate(i * 45);
            p.drawLine(0, -27, 0, -40);
            p.restore();
        }
        break;
    }
    case Ic::Play: {
        fill();
        QPainterPath t; t.moveTo(30, 16); t.lineTo(84, 50); t.lineTo(30, 84); t.closeSubpath();
        p.drawPath(t);
        break;
    }
    case Ic::Pause: {
        fill();
        p.drawRoundedRect(QRectF(24, 16, 20, 68), 5, 5);
        p.drawRoundedRect(QRectF(56, 16, 20, 68), 5, 5);
        break;
    }
    case Ic::Fullscreen:
        for (int sx : {0, 1})
            for (int sy : {0, 1}) {
                p.save();
                p.translate(50, 50);
                p.scale(sx ? 1 : -1, sy ? 1 : -1);
                p.drawLine(42, 22, 42, 42);
                p.drawLine(42, 42, 22, 42);
                p.restore();
            }
        break;
    case Ic::ExitFullscreen:
        for (int sx : {0, 1})
            for (int sy : {0, 1}) {
                p.save();
                p.translate(50, 50);
                p.scale(sx ? 1 : -1, sy ? 1 : -1);
                p.drawLine(22, 42, 22, 22);
                p.drawLine(22, 22, 42, 22);
                p.restore();
            }
        break;
    case Ic::Scissors: {
        p.drawEllipse(QPointF(28, 76), 12, 12);
        p.drawEllipse(QPointF(72, 76), 12, 12);
        p.drawLine(36, 67, 74, 14);
        p.drawLine(64, 67, 26, 14);
        break;
    }
    case Ic::Trash: {
        p.drawLine(16, 28, 84, 28);
        p.drawLine(38, 28, 38, 16);
        p.drawLine(38, 16, 62, 16);
        p.drawLine(62, 16, 62, 28);
        QPainterPath b; b.moveTo(24, 28); b.lineTo(29, 84); b.lineTo(71, 84); b.lineTo(76, 28);
        p.drawPath(b);
        p.drawLine(42, 42, 43, 70);
        p.drawLine(58, 42, 57, 70);
        break;
    }
    case Ic::TrimStart:
        p.drawLine(16, 18, 16, 82);
        p.drawLine(32, 50, 86, 50);
        p.drawLine(32, 50, 50, 32);
        p.drawLine(32, 50, 50, 68);
        break;
    case Ic::TrimEnd:
        p.drawLine(84, 18, 84, 82);
        p.drawLine(68, 50, 14, 50);
        p.drawLine(68, 50, 50, 32);
        p.drawLine(68, 50, 50, 68);
        break;
    case Ic::Blur: {
        p.drawRoundedRect(QRectF(12, 16, 76, 68), 10, 10);
        for (int i = 0; i < 4; ++i) {
            QColor cc = c;
            cc.setAlphaF(1.0 - i * 0.24);
            QPen q(cc, 7, Qt::SolidLine, Qt::RoundCap);
            p.setPen(q);
            p.drawLine(28, 32 + i * 12, 72, 32 + i * 12);
        }
        break;
    }
    case Ic::Image: {
        p.drawRoundedRect(QRectF(10, 16, 80, 68), 10, 10);
        fill();
        p.drawEllipse(QPointF(33, 38), 7, 7);
        stroke();
        QPainterPath m; m.moveTo(14, 74); m.lineTo(38, 52); m.lineTo(54, 68); m.lineTo(66, 56); m.lineTo(86, 74);
        p.drawPath(m);
        break;
    }
    case Ic::Undo:
    case Ic::Redo: {
        if (id == Ic::Redo) { p.translate(100, 0); p.scale(-1, 1); }
        QPainterPath a; a.moveTo(24, 46); a.cubicTo(44, 24, 86, 30, 80, 76);
        p.drawPath(a);
        p.drawLine(24, 46, 24, 22);
        p.drawLine(24, 46, 48, 46);
        break;
    }
    case Ic::Pen: {
        // Stift schräg, darunter eine Schwungline
        p.drawLine(30, 64, 70, 24);
        p.drawLine(70, 24, 80, 34);
        p.drawLine(80, 34, 40, 74);
        p.drawLine(40, 74, 26, 78);
        p.drawLine(26, 78, 30, 64);
        QPainterPath w; w.moveTo(14, 90); w.cubicTo(34, 82, 52, 96, 86, 86);
        p.drawPath(w);
        break;
    }
    case Ic::Shapes: {
        p.drawRoundedRect(QRectF(12, 14, 44, 44), 6, 6);
        p.drawEllipse(QPointF(64, 64), 24, 24);
        break;
    }
    case Ic::Volume: {
        fill();
        QPainterPath s; s.moveTo(12, 38); s.lineTo(30, 38); s.lineTo(52, 18); s.lineTo(52, 82); s.lineTo(30, 62); s.lineTo(12, 62);
        s.closeSubpath();
        p.drawPath(s);
        stroke();
        p.drawArc(QRectF(42, 32, 26, 36), -60 * 16, 120 * 16);
        p.drawArc(QRectF(42, 18, 46, 64), -60 * 16, 120 * 16);
        break;
    }
    }
    p.end();
    return QIcon(pm);
}

QPixmap makeAppPixmap(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 100.0, size / 100.0);
    QLinearGradient g(0, 0, 100, 100);
    g.setColorAt(0, QColor("#22d3a0"));
    g.setColorAt(1, QColor("#2563eb"));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(QRectF(4, 4, 92, 92), 22, 22);
    p.setBrush(QColor(255, 255, 255, 235));
    QPainterPath t; t.moveTo(38, 27); t.lineTo(74, 50); t.lineTo(38, 73); t.closeSubpath();
    p.drawPath(t);
    p.setPen(QPen(QColor(255, 255, 255, 200), 5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(24, 20, 24, 80);
    return pm;
}
