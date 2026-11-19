#include "overlay.h"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include "textrender.h"
#include "theme.h"

Overlay::Overlay(int itemId, EditorHost* host) : id_(itemId), host_(host) {
    Project& pr = host_->pr();
    Item* it = pr.item(id_);
    if (it->kind == Item::Image) pm_ = QPixmap(it->path);
    w_ = it->w * pr.vw;
    h_ = it->h * pr.vh;
    setPos(it->x * pr.vw, it->y * pr.vh);
    setZValue(10);
}

QRectF Overlay::handleRect() const {
    double s = 14.0 / std::max(0.01, host_->viewScale());
    return QRectF(w_ - s, h_ - s, s, s);
}

QRectF Overlay::boundingRect() const { return QRectF(0, 0, w_, h_).adjusted(-3, -3, 3, 3); }

// Echte Unschärfe in der Vorschau: Ausschnitt des aktuellen Bildes stark verkleinern und weich wieder aufziehen.
void Overlay::paintBlur(QPainter* p) {
    Item* it = host_->pr().item(id_);
    QImage img = host_->frameImage();
    QRectF vr = host_->videoRect();
    QRectF me(pos(), QSizeF(w_, h_));
    QRectF sr = me & vr;
    if (img.isNull() || sr.isEmpty() || !it) return;
    const double k = img.width() / vr.width();
    QRect src = QRectF((sr.x() - vr.x()) * k, (sr.y() - vr.y()) * k, sr.width() * k, sr.height() * k)
                    .toAlignedRect() & img.rect();
    if (src.isEmpty()) return;
    QImage crop = img.copy(src);
    const double f = std::max(2.0, it->strength * img.height() / 1080.0 / 2.2);
    QImage small = crop.scaled(std::max(2, int(crop.width() / f)), std::max(2, int(crop.height() / f)),
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage mid = small.scaled(small.width() * 2, small.height() * 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->drawImage(sr.translated(-pos()), mid);
}

void Overlay::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    const Theme& th = currentTheme();
    const double sc = std::max(0.01, host_->viewScale());
    const QRectF r(0, 0, w_, h_);
    const bool sel = host_->selItem() == id_;
    Item* it = host_->pr().item(id_);
    if (it && it->kind == Item::Text) {
        // Text in der echten Pixelgröße des Rahmens rendern (wie im Export), nur bei Änderungen neu
        const QSize px(std::max(2, int(w_)), std::max(2, int(h_)));
        const QString key = QString("%1|%2|%3|%4|%5|%6x%7").arg(it->text, it->font).arg(it->color).arg(it->bg)
                                .arg(it->bgColor).arg(px.width()).arg(px.height());
        if (key != textKey_) { textImg_ = renderTextImage(*it, px); textKey_ = key; }
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->drawImage(r, textImg_);
    } else if (it && it->graphic()) {
        // Zeichnung / Form wie im Export rendern, nur bei Änderungen neu
        const QSize px(std::max(2, int(w_)), std::max(2, int(h_)));
        int pts = 0;
        for (const QPolygonF& s : it->strokes) pts += s.size();
        const QString key = QString("g|%1|%2|%3|%4|%5|%6|%7x%8").arg(it->kind).arg(it->shape).arg(it->color).arg(it->stroke)
                                .arg(it->fill).arg(pts).arg(px.width()).arg(px.height());
        if (key != textKey_) { textImg_ = renderGraphicImage(*it, px, host_->pr().vh); textKey_ = key; }
        p->drawImage(r, textImg_);
    } else if (it && it->kind == Item::Video) {
        // das Bild kommt vom eigenen Player darunter, hier nur Rahmen und Griffe
    } else if (!pm_.isNull()) {
        p->setOpacity(it ? std::clamp(it->opacity, 0.0, 1.0) : 1.0);
        p->drawPixmap(r, pm_, QRectF(pm_.rect()));
        p->setOpacity(1.0);
    } else {
        paintBlur(p);
    }

    if (!host_->showGuides()) return;
    QPen pen(sel ? th.accent : QColor(255, 255, 255, 170), (sel ? 2.5 : 1.5) / sc);
    pen.setStyle(sel ? Qt::SolidLine : Qt::DashLine);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawRect(r);
    if (sel) {
        p->fillRect(handleRect(), th.accent);
        // kleine Ecken als Griffe
        double s = 8.0 / sc;
        for (QPointF c : {QPointF(0, 0), QPointF(w_, 0), QPointF(0, h_)})
            p->fillRect(QRectF(c.x() - s / 2, c.y() - s / 2, s, s), th.accent);
    }
}

void Overlay::mousePressEvent(QGraphicsSceneMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return e->ignore();
    host_->selectItem(id_);
    mode_ = handleRect().contains(e->pos()) ? Resize : Move;
    startScene_ = e->scenePos();
    startPos_ = pos();
    startW_ = w_;
    startH_ = h_;
    moved_ = false;
    e->accept();
}

void Overlay::mouseMoveEvent(QGraphicsSceneMouseEvent* e) {
    if (mode_ == None) return;
    if (!moved_) { host_->pushUndo(); moved_ = true; }
    Project& pr = host_->pr();
    Item* it = pr.item(id_);
    if (!it) return;
    const QPointF d = e->scenePos() - startScene_;
    prepareGeometryChange();
    if (mode_ == Move) {
        setPos(std::clamp(startPos_.x() + d.x(), 0.0, std::max(0.0, pr.vw - w_)),
               std::clamp(startPos_.y() + d.y(), 0.0, std::max(0.0, pr.vh - h_)));
    } else {
        double nw = std::clamp(startW_ + d.x(), 16.0, pr.vw - startPos_.x());
        double nh = it->media() ? nw * it->ar
                                            : std::clamp(startH_ + d.y(), 16.0, pr.vh - startPos_.y());
        w_ = nw;
        h_ = nh;
    }
    syncModel();
    host_->overlayMoved();
}

void Overlay::mouseReleaseEvent(QGraphicsSceneMouseEvent*) {
    mode_ = None;
    host_->modelEdited(true);
}

void Overlay::syncModel() {
    Project& pr = host_->pr();
    Item* it = pr.item(id_);
    if (!it) return;
    it->x = pos().x() / pr.vw;
    it->y = pos().y() / pr.vh;
    it->w = w_ / pr.vw;
    it->h = h_ / pr.vh;
}
