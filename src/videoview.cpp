#include "videoview.h"

#include <QGraphicsItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include "theme.h"

VideoView::VideoView(QGraphicsScene* scene, QWidget* parent) : QGraphicsView(scene, parent) {
    setRenderHint(QPainter::Antialiasing);
    // Videobild beim Verkleinern/Vergrößern glätten. Ohne das skaliert der Software-Pfad (Linux, kein GPU-Umweg)
    // per Nearest-Neighbor: Kanten werden treppig, kleine Schrift im Video unlesbar - "schlechte Auflösung".
    setRenderHint(QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    // Hineingezogene Dateien nicht selbst schlucken: das Hauptfenster entscheidet, was damit passiert
    setAcceptDrops(false);
    viewport()->setAcceptDrops(false);
}

void VideoView::setAmbient(const QImage& tiny) {
    ambient_ = tiny;
    viewport()->update();
}

void VideoView::setToolActive(bool on) {
    tool_ = on;
    toolDown_ = false;
    if (on) viewport()->setCursor(Qt::CrossCursor);
    else viewport()->unsetCursor();
}

// Cover für den Hintergrund: so weich wie bei Windows, die Formen des Bildes bleiben erkennbar (rund 2 % der Breite)
static QImage blurred(const QImage& src) {
    const int n = 512;
    const QImage s = src.scaled(n, n, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return softBlur(s.copy((s.width() - n) / 2, (s.height() - n) / 2, n, n), 10);
}

void VideoView::setAudioInfo(const QString& title, const QString& subtitle, const QImage& cover) {
    audioTitle_ = title;
    audioSub_ = subtitle;
    cover_ = cover.isNull() ? QImage() : cover.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverBlur_ = cover.isNull() ? QImage() : blurred(cover);
    viewport()->update();
}

// Hintergrund einer Tondatei: Cover groß und verschwommen, sonst ein ruhiger dunkler Verlauf mit etwas Akzentfarbe
static void paintAudioBackground(QPainter* p, const QRect& vr, const QImage& blur) {
    p->fillRect(vr, QColor("#111318"));
    if (!blur.isNull()) {
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        const double side = std::max(vr.width(), vr.height()) * 1.1;  // füllt alles, Ränder des Weichzeichners außerhalb
        p->drawImage(QRectF(vr.center().x() - side / 2, vr.center().y() - side / 2, side, side), blur);
        p->fillRect(vr, QColor(40, 40, 44, 150));  // grauer Schleier statt Schwarz: ruhig, Formen bleiben sichtbar
    } else {
        QColor a = currentTheme().accent;
        for (const auto& [x, y, al] : {std::tuple{0.78, 0.18, 46}, {0.15, 0.85, 34}, {0.45, 0.55, 18}}) {
            QRadialGradient g(QPointF(vr.left() + x * vr.width(), vr.top() + y * vr.height()), std::max(vr.width(), vr.height()) * 0.55);
            a.setAlpha(al);
            g.setColorAt(0, a);
            a.setAlpha(0);
            g.setColorAt(1, a);
            p->fillRect(vr, g);
        }
    }
    // unten dunkler, damit Titel und Cover-Karte gut lesbar sind
    QLinearGradient shade(0, vr.top() + vr.height() * 0.45, 0, vr.bottom());
    shade.setColorAt(0, QColor(0, 0, 0, 0));
    shade.setColorAt(1, QColor(0, 0, 0, 120));
    p->fillRect(vr, shade);
}

// Zwei verbundene Achtelnoten als Linienzeichnung (ohne Cover)
static void paintNote(QPainter* p, const QRectF& card) {
    const QPointF c = card.center();
    const double k = card.width() / 112.0;
    QPen pen(QColor(236, 238, 243), 4.6 * k, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    auto head = [&](QPointF at) {
        p->save();
        p->translate(at);
        p->rotate(-22);
        p->drawEllipse(QPointF(0, 0), 10.5 * k, 7.8 * k);
        p->restore();
    };
    const QPointF h1(c.x() - 15 * k, c.y() + 19 * k), h2(c.x() + 19 * k, c.y() + 12 * k);
    head(h1);
    head(h2);
    const double x1 = h1.x() + 9.6 * k, x2 = h2.x() + 9.6 * k;
    p->drawLine(QPointF(x1, h1.y() - 3 * k), QPointF(x1, c.y() - 24 * k));
    p->drawLine(QPointF(x2, h2.y() - 3 * k), QPointF(x2, c.y() - 31 * k));
    QPainterPath beam;  // Balken oben, leicht schräg
    beam.moveTo(x1, c.y() - 24 * k);
    beam.lineTo(x2, c.y() - 31 * k);
    p->setPen(QPen(QColor(236, 238, 243), 7 * k, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawPath(beam);
}

// Tondatei: Cover-Karte unten links, daneben Titel und Interpret/Album
void VideoView::drawForeground(QPainter* p, const QRectF&) {
    if (audioTitle_.isEmpty()) return;
    p->save();
    p->resetTransform();
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF vr = viewport()->rect();
    const double m = std::clamp(std::min(vr.width(), vr.height()) * 0.045, 12.0, 32.0);
    const double cs = std::clamp(std::min(vr.height() * 0.36, vr.width() * 0.3), 72.0, 300.0);
    const QRectF card(vr.left() + m, vr.bottom() - m - cs, cs, cs);
    const double rad = cs * 0.06;
    // Schatten
    for (int i = 6; i >= 1; --i) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 14));
        p->drawRoundedRect(card.adjusted(-i * 1.5, -i * 0.5, i * 1.5, i * 2.5), rad + i, rad + i);
    }
    QPainterPath clip;
    clip.addRoundedRect(card, rad, rad);
    if (!cover_.isNull()) {
        p->save();
        p->setClipPath(clip);
        const QImage c = cover_.scaled(card.size().toSize() * devicePixelRatioF(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRectF src((c.width() - card.width() * devicePixelRatioF()) / 2, (c.height() - card.height() * devicePixelRatioF()) / 2,
                         card.width() * devicePixelRatioF(), card.height() * devicePixelRatioF());
        p->drawImage(card, c, src);
        p->restore();
        p->setPen(QPen(QColor(255, 255, 255, 30), 1));
        p->setBrush(Qt::NoBrush);
        p->drawPath(clip);
    } else {
        QLinearGradient g(card.topLeft(), card.bottomRight());
        g.setColorAt(0, QColor(54, 58, 68, 235));
        g.setColorAt(1, QColor(30, 32, 38, 235));
        p->setPen(QPen(QColor(255, 255, 255, 38), 1));
        p->setBrush(g);
        p->drawPath(clip);
        paintNote(p, card);
    }
    // Titel und Interpret
    const double tx = card.right() + m * 0.9;
    const double tw = vr.right() - m - tx;
    if (tw > 60) {
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(int(std::clamp(cs * 0.15, 15.0, 36.0)));
        QFont fs = font();
        fs.setPixelSize(int(std::clamp(cs * 0.085, 12.0, 20.0)));
        const QFontMetricsF mf(f), ms(fs);
        double y = card.bottom() - (audioSub_.isEmpty() ? 0 : ms.height() + 4);
        p->setFont(f);
        p->setPen(QColor(255, 255, 255, 245));
        p->drawText(QPointF(tx, y - mf.descent()), mf.elidedText(audioTitle_, Qt::ElideRight, tw));
        if (!audioSub_.isEmpty()) {
            p->setFont(fs);
            p->setPen(QColor(255, 255, 255, 175));
            p->drawText(QPointF(tx, card.bottom() - ms.descent()), ms.elidedText(audioSub_, Qt::ElideRight, tw));
        }
    }
    p->restore();
}

// Windows XP: sanfte Hügellandschaft mit Himmel ("Bliss"-Stimmung), wenn noch kein Video geladen ist
void VideoView::paintBliss(QPainter* p, const QRect& r) {
    p->setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient sky(0, r.top(), 0, r.bottom());
    sky.setColorAt(0.0, QColor("#1f5fcf"));
    sky.setColorAt(0.45, QColor("#5b9bee"));
    sky.setColorAt(0.75, QColor("#a9cff7"));
    sky.setColorAt(1.0, QColor("#d8ecff"));
    p->fillRect(r, sky);
    // Wolken
    auto cloud = [&](double cx, double cy, double w, double h, int a) {
        QRadialGradient g(QPointF(r.left() + cx * r.width(), r.top() + cy * r.height()), w * r.width());
        g.setColorAt(0, QColor(255, 255, 255, a));
        g.setColorAt(1, QColor(255, 255, 255, 0));
        p->save();
        p->translate(r.left() + cx * r.width(), r.top() + cy * r.height());
        p->scale(1.0, h / w);
        p->translate(-(r.left() + cx * r.width()), -(r.top() + cy * r.height()));
        p->fillRect(r.adjusted(-r.width(), -r.height(), r.width(), r.height()), g);
        p->restore();
    };
    cloud(0.22, 0.28, 0.22, 0.07, 170);
    cloud(0.62, 0.18, 0.28, 0.06, 150);
    cloud(0.86, 0.36, 0.16, 0.05, 130);
    // Hügel
    const double W = r.width(), H = r.height();
    QPainterPath back;
    back.moveTo(r.left(), r.top() + H * 0.78);
    back.cubicTo(r.left() + W * 0.25, r.top() + H * 0.62, r.left() + W * 0.55, r.top() + H * 0.70, r.left() + W, r.top() + H * 0.66);
    back.lineTo(r.right() + 1, r.bottom() + 1);
    back.lineTo(r.left(), r.bottom() + 1);
    back.closeSubpath();
    QLinearGradient gb(0, r.top() + H * 0.62, 0, r.bottom());
    gb.setColorAt(0, QColor("#7cc242"));
    gb.setColorAt(1, QColor("#3f8a1f"));
    p->fillPath(back, gb);
    QPainterPath front;
    front.moveTo(r.left(), r.top() + H * 0.70);
    front.cubicTo(r.left() + W * 0.30, r.top() + H * 0.50, r.left() + W * 0.66, r.top() + H * 0.58, r.left() + W, r.top() + H * 0.92);
    front.lineTo(r.right() + 1, r.bottom() + 1);
    front.lineTo(r.left(), r.bottom() + 1);
    front.closeSubpath();
    QLinearGradient gf(r.left() + W * 0.2, r.top() + H * 0.52, r.left() + W * 0.5, r.bottom());
    gf.setColorAt(0, QColor("#a3dc5a"));
    gf.setColorAt(0.35, QColor("#5fae2e"));
    gf.setColorAt(1, QColor("#2d6f14"));
    p->fillPath(front, gf);
}

void VideoView::drawBackground(QPainter* p, const QRectF&) {
    p->save();
    p->resetTransform();
    const QRect vr = viewport()->rect();
    if (!audioTitle_.isEmpty()) {
        paintAudioBackground(p, vr, coverBlur_);
        p->restore();
        return;
    }
    p->fillRect(vr, currentTheme().video);
    if (currentTheme().xp && ambient_.isNull()) paintBliss(p, vr);
    if (currentTheme().acryl && ambient_.isNull()) {
        const QImage& smoke = acrylSmoke(currentTheme());
        if (smoke_.size() != vr.size() || smokeKey_ != smoke.pixel(0, 0)) {
            QPixmap pm = QPixmap::fromImage(smoke.scaled(vr.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            QPainter shade(&pm);
            shade.fillRect(pm.rect(), currentTheme().bg.lightness() < 128 ? QColor(0, 0, 0, 90) : QColor(255, 255, 255, 90));
            shade.end();
            smoke_ = pm;
            smokeKey_ = smoke.pixel(0, 0);
        }
        p->drawPixmap(vr.topLeft(), smoke_);
    }
    if (!ambient_.isNull()) {
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->setOpacity(0.5);
        p->drawImage(vr, ambient_);
        p->setOpacity(1.0);
        p->fillRect(vr, QColor(0, 0, 0, 70));
    }
    p->restore();
}

void VideoView::mousePressEvent(QMouseEvent* e) {
    if (tool_) {
        if (e->button() == Qt::LeftButton) {
            toolDown_ = true;
            emit toolPressed(mapToScene(e->pos()));
        }
        return;
    }
    QGraphicsItem* it = itemAt(e->pos());
    pressOnEmpty_ = e->button() == Qt::LeftButton && (!it || it->zValue() < 5 || it->acceptedMouseButtons() == Qt::NoButton);
    pressPos_ = e->pos();
    QGraphicsView::mousePressEvent(e);
}

void VideoView::mouseReleaseEvent(QMouseEvent* e) {
    if (tool_) {
        if (toolDown_ && e->button() == Qt::LeftButton) {
            toolDown_ = false;
            emit toolReleased(mapToScene(e->pos()));
        }
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
    if (pressOnEmpty_ && (e->pos() - pressPos_).manhattanLength() < 6) emit emptyClicked();
    pressOnEmpty_ = false;
}

void VideoView::mouseMoveEvent(QMouseEvent* e) {
    if (tool_) {
        if (toolDown_) emit toolMoved(mapToScene(e->pos()));
        emit mouseActivity();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
    emit mouseActivity();
}
