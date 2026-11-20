#include "timeline.h"

#include <QFileInfo>
#include <QMimeData>
#include <QContextMenuEvent>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>
#include "i18n.h"
#include "theme.h"
#include "icons.h"

static constexpr double kMinPiece = 0.1;

// Lautstärke wie in Schnittprogrammen üblich in dB: -60 dB = stumm, oben +15,6 dB = 600 %
static const double kMaxDb = 20.0 * std::log10(6.0);
static double toDb(double v) { return v <= 0.001 ? -60.0 : std::clamp(20.0 * std::log10(v), -60.0, kMaxDb); }
static double fromDb(double db) { return db <= -59.9 ? 0.0 : std::pow(10.0, db / 20.0); }

Timeline::Timeline(EditorHost* host) : host_(host) {
    setAcceptDrops(true);  // Medien hineinziehen
    setMouseTracking(true);
    setMinimumHeight(RULER + ROW + LANE * 2 + 16);
}

double Timeline::basePps() const {
    const double vw = host_->timelineViewportWidth() - 2 * LEFT;
    return std::max(0.001, vw / std::max(host_->pr().total(), 0.5));
}

double Timeline::pps() const { return frozen_ ? frozenPps_ : basePps() * zoom_; }

static bool onVTrack(const Project& pr, const Item& it) {
    const Track* t = pr.track(it.track);
    return t && !t->audio && it.track != kMainTrack;
}

QList<Timeline::Row> Timeline::rows() const {
    const Project& pr = host_->pr();
    QList<Row> r;
    r << Row{Row::Main, kMainTrack, double(RULER), double(ROW)};
    double y = RULER + ROW + 6;
    for (int id : pr.trackIds(false))
        if (id != kMainTrack) { r << Row{Row::VTrack, id, y, double(LANE)}; y += LANE; }
    for (const Item& it : pr.items)
        if (!onVTrack(pr, it)) { r << Row{Row::Lane, it.id, y, double(LANE)}; y += LANE; }
    if (!pr.subs.isEmpty()) { r << Row{Row::Subs, 0, y, double(LANE)}; y += LANE; }
    for (int id : pr.trackIds(true)) { r << Row{Row::ATrack, id, y, double(LANE)}; y += LANE; }
    return r;
}

int Timeline::rowAt(double y, const QList<Row>& rs) const {
    for (int i = 0; i < rs.size(); ++i)
        if (y >= rs[i].y && y < rs[i].y + rs[i].h) return i;
    return -1;
}

int Timeline::targetTrack(double y, bool audio) const {
    const QList<Row> rs = rows();
    const int i = rowAt(y, rs);
    if (i < 0) return 0;
    if (audio) return rs[i].kind == Row::ATrack ? rs[i].id : 0;
    return rs[i].kind == Row::VTrack || rs[i].kind == Row::Main ? rs[i].id : 0;
}

QRectF Timeline::itemRect(const Item& it, const QList<Row>& rs) const {
    const bool tr = onVTrack(host_->pr(), it);
    for (const Row& r : rs)
        if ((tr && r.kind == Row::VTrack && r.id == it.track) || (!tr && r.kind == Row::Lane && r.id == it.id))
            return QRectF(xOf(it.t0), r.y + 2, std::max(6.0, (it.t1 - it.t0) * pps()), LANE - 4);
    return {};
}

QRectF Timeline::cueRect(int i, const QList<Row>& rs) const {
    const Project& pr = host_->pr();
    for (const Row& r : rs)
        if (r.kind == Row::Subs) {
            const SubCue& c = pr.subs[i];
            return QRectF(xOf(c.t0), r.y + 2, std::max(4.0, (c.t1 - c.t0) * pps()), LANE - 4);
        }
    return {};
}

int Timeline::cueAt(const QPointF& pos) const {
    const QList<Row> rs = rows();
    const Project& pr = host_->pr();
    for (int i = 0; i < pr.subs.size(); ++i)
        if (cueRect(i, rs).adjusted(-3, 0, 3, 0).contains(pos)) return i;
    return -1;
}

void Timeline::relayout() {
    Project& pr = host_->pr();
    {
        const QList<Row> rs = rows();
        const int h = int(rs.last().y + rs.last().h + 16);
        setMinimumHeight(h);
        if (height() != std::max(h, host_->timelineViewportHeight())) resize(width(), std::max(h, host_->timelineViewportHeight()));
        if (header_) header_->update();
    }
    if (frozen_) {  // beim Kürzen: Maßstab bleibt, nur breiter werden, nie schmaler (sonst springt alles)
        const double w = std::max(pr.total() * pps(), fx1_.isEmpty() ? 0.0 : fx1_.last() - LEFT) + 2 * LEFT + 40;
        if (w > width()) setFixedWidth(int(std::min(16000000.0, w)));
        update();
        return;
    }
    // höchstens ~250 Pixel pro Sekunde, auch bei stundenlangen Aufnahmen
    zoom_ = std::clamp(zoom_, 1.0, std::max(1.0, 250.0 / basePps()));
    const double w = pr.total() * pps() + 2 * LEFT;
    setFixedWidth(int(std::min(16000000.0, std::max(w, double(host_->timelineViewportWidth())))));
    update();
}

// x des Randes, der gerade gekürzt wird (dort steht beim Kürzen die Abspielmarke)
double Timeline::trimEdgeX() const {
    const QRectF r = pieceRect(drag_.index);
    return drag_.side == 'l' ? r.left() - 1 : r.right() + 1;
}

int Timeline::reorderTarget() const {
    Project& pr = host_->pr();
    int target = 0;
    for (int i = 0; i < pr.pieces.size(); ++i) {
        if (i == drag_.index) continue;
        const QRectF r = pieceRect(i);
        if (r.center().x() < mouseX_) ++target;
    }
    return target;
}

void Timeline::zoomBy(double factor) {
    zoom_ *= factor;
    relayout();
    host_->timelineZoomed();
}

static QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}

QRectF Timeline::pieceRect(int i) const {
    Project& pr = host_->pr();
    double x0 = xOf(pr.outStart(i)), x1 = xOf(pr.outStart(i) + pr.pieces[i].outDur());
    if (frozen_ && i < fx0_.size()) {
        const double len = pr.pieces[i].outDur() * pps();
        const double grow = pr.pieces.value(drag_.index).outDur() - origOut_;  // > 0: Abschnitt wird länger
        if (i == drag_.index) {
            // der gezogene Rand wandert, der andere bleibt stehen
            if (drag_.side == 'l') { x1 = fx1_[i]; x0 = x1 - len; }
            else { x0 = fx0_[i]; x1 = x0 + len; }
        } else if (i > drag_.index && drag_.side == 'r' && grow > 0) {
            x0 = fx0_[i] + grow * pps();  // länger nach rechts: die folgenden Abschnitte weichen aus
            x1 = fx1_[i] + grow * pps();
        } else {
            x0 = fx0_[i];
            x1 = fx1_[i];
        }
    }
    return QRectF(x0 + 1, RULER + 2, std::max(2.0, x1 - x0 - 2), ROW - 6);
}

QRectF Timeline::waveRect(int i) const {
    const QRectF r = pieceRect(i);
    return QRectF(r.left(), r.bottom() - WAVE, r.width(), WAVE);
}

QRectF Timeline::audioRect(int k) const {
    Project& pr = host_->pr();
    const AudioClip& a = pr.audios[k];
    const int tid = pr.audioTrackOf(a);
    for (const Row& r : rows())
        if (r.kind == Row::ATrack && r.id == tid) return QRectF(xOf(a.t0), r.y + 2, std::max(6.0, a.dur * pps()), LANE - 4);
    return {};
}

double Timeline::volLineY(const QRectF& area, double volume) const {
    const double frac = (toDb(volume) + 60.0) / (60.0 + kMaxDb);
    return area.bottom() - 3 - frac * (area.height() - 6);
}

void Timeline::drawWave(QPainter& p, const QRectF& area, const WaveSet* ws, double srcAtLeft, double srcPerPx,
                        double volume, bool centered, const QColor& color, const QRect& vis) const {
    if (!ws || ws->peaks.empty()) return;
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::FlatCap));
    const double from = std::max(area.left() + 1, double(vis.left()));
    const double to = std::min(area.right() - 1, double(vis.right()));
    const int n = int(ws->peaks.size());
    for (double x = std::floor(from / 2) * 2; x < to; x += 2) {
        const double s0 = srcAtLeft + (x - area.left()) * srcPerPx;
        int i0 = int(s0 * ws->rate), i1 = std::max(i0 + 1, int((s0 + 2 * srcPerPx) * ws->rate));
        if (i0 >= n) break;
        if (i1 <= 0) continue;
        i0 = std::max(0, i0);
        i1 = std::min(n, i1);
        float amp = 0;
        for (int i = i0; i < i1; ++i) amp = std::max(amp, ws->peaks[i]);
        const double h = std::min(1.0, std::pow(amp * volume, 0.7)) * (area.height() - 4);
        if (h < 0.6) continue;
        if (centered) p.drawLine(QPointF(x, area.center().y() - h / 2), QPointF(x, area.center().y() + h / 2));
        else p.drawLine(QPointF(x, area.bottom() - 2), QPointF(x, area.bottom() - 2 - h));
    }
}

void Timeline::drawDbPill(QPainter& p, double x, double y, double volume) const {
    const double db = toDb(volume);
    const QString t = db <= -59.9 ? QStringLiteral("-∞ dB") : QString("%1%2 dB").arg(db > 0.05 ? "+" : "").arg(db, 0, 'f', 1);
    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    const QFontMetrics fm(f);
    QRectF pill(x - fm.horizontalAdvance(t) / 2.0 - 7, y - 24, fm.horizontalAdvance(t) + 14, 18);
    pill.moveLeft(std::clamp(pill.left(), 2.0, width() - pill.width() - 2));
    pill.moveTop(std::max(1.0, pill.top()));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 24, 235));
    p.drawRoundedRect(pill, 6, 6);
    p.setPen(Qt::white);
    p.drawText(pill, Qt::AlignCenter, t);
}

void Timeline::paintEvent(QPaintEvent* ev) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Theme& th = currentTheme();
    if (!th.acryl) p.fillRect(rect(), th.timeline);  // Acryl: Fläche kommt aus dem Stylesheet, der Rauch scheint durch
    Project& pr = host_->pr();
    const double sc = pps(), total = pr.total();
    const QRect vis = ev->rect();

    // Lineal
    double step = 3600;
    for (double s : {0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0, 600.0, 1200.0, 1800.0})
        if (s * sc >= 70) { step = s; break; }
    QFont small = font();
    small.setPixelSize(11);
    p.setFont(small);
    const double tFrom = std::max(0.0, std::floor(tOf(vis.left() - 80) / step) * step);
    for (double t = tFrom; t <= total + 0.001 && xOf(t) < vis.right() + 80; t += step) {
        int x = int(xOf(t));
        p.setPen(th.border);
        p.drawLine(x, RULER - 8, x, RULER - 1);
        p.setPen(th.muted);
        p.drawText(x + 4, 14, fmtTime(t));
        for (int m = 1; m < 5; ++m) {
            int xm = int(xOf(t + step * m / 5.0));
            p.setPen(th.border);
            p.drawLine(xm, RULER - 4, xm, RULER - 1);
        }
    }

    // Video-Abschnitte: Vorschaubilder oben, Ton-Wellenform mit Lautstärke-Linie unten
    const QColor pieceBase = mix(th.panel2, th.accent, 0.35);
    const QColor waveBg = mix(th.timeline, th.accent, 0.22);
    const QColor waveFg = mix(th.accent, QColor(Qt::white), 0.25);
    auto drawPiece = [&](int i, const QRectF& r) {
        const Piece& pc = pr.pieces[i];
        if (r.right() < vis.left() - 4 || r.left() > vis.right() + 4) return;
        const QRectF rt(r.left(), r.top(), r.width(), r.height() - WAVE);
        const QRectF rw(r.left(), r.bottom() - WAVE, r.width(), WAVE);
        const bool sel = i == host_->selPiece();
        QPainterPath clip;
        clip.addRoundedRect(r, 7, 7);
        p.save();
        p.setClipPath(clip);
        p.fillRect(r, pieceBase);
        if (const ThumbSet* ts = host_->thumbs(pc.src); ts && !ts->imgs.isEmpty()) {
            const QImage& first = ts->imgs.first();
            const double tw = rt.height() * first.width() / std::max(1, first.height());
            const double from = std::max(rt.left(), double(vis.left()) - tw);
            const double to = std::min(rt.right(), double(vis.right()) + tw);
            for (double tx = rt.left() + std::floor((from - rt.left()) / tw) * tw; tx < to; tx += tw) {
                double tOut = (tx + tw / 2 - rt.left()) / sc;
                double src = pc.start + std::clamp(tOut, 0.0, pc.outDur()) * pc.speed;
                int idx = std::clamp(int(src / ts->step), 0, int(ts->imgs.size()) - 1);
                p.drawImage(QRectF(tx, rt.top(), tw + 1, rt.height()), ts->imgs[idx]);
            }
        }
        QLinearGradient shade(0, rt.top(), 0, rt.bottom());
        shade.setColorAt(0, QColor(0, 0, 0, 10));
        shade.setColorAt(1, QColor(0, 0, 0, 120));
        p.fillRect(rt, shade);
        p.fillRect(rw, waveBg);
        drawWave(p, rw, host_->waves(pr.sources.value(pc.src).path), pc.start, pc.speed / sc, pc.volume, false,
                 waveFg, vis);
        // Lautstärke-Linie
        const double ly = volLineY(rw, pc.volume);
        const bool hot = (hover_ == Hover::Piece && hoverIndex_ == i) || (drag_.kind == Drag::PieceVol && drag_.index == i);
        p.setPen(QPen(QColor(255, 255, 255, hot ? 255 : 170), hot ? 2 : 1.3));
        p.drawLine(QPointF(rw.left() + 2, ly), QPointF(rw.right() - 2, ly));
        if (sel) p.fillRect(r, QColor(th.accent.red(), th.accent.green(), th.accent.blue(), 40));
        p.restore();
        p.setBrush(Qt::NoBrush);
        p.setPen(sel ? QPen(th.accent, 2.5) : QPen(QColor(0, 0, 0, 90), 1));
        p.drawRoundedRect(r, 7, 7);
        // Griffe zum Kürzen (an jedem Abschnitt)
        p.setPen(Qt::NoPen);
        p.setBrush(sel ? th.accent : QColor(255, 255, 255, 150));
        p.drawRoundedRect(QRectF(r.left() - 1, rt.center().y() - 11, 4, 22), 2, 2);
        p.drawRoundedRect(QRectF(r.right() - 3, rt.center().y() - 11, 4, 22), 2, 2);
        QString label = fmtTime(pc.outDur());
        if (pc.speed != 1.0) label += QString("  ×%1").arg(pc.speed, 0, 'g', 3);
        QFontMetrics fm(small);
        p.setFont(small);
        QRectF pill(r.left() + 8, rt.top() + 5, fm.horizontalAdvance(label) + 12, 16);
        if (pill.right() < r.right() - 6) {
            p.setBrush(QColor(0, 0, 0, 150));
            p.drawRoundedRect(pill, 8, 8);
            p.setPen(Qt::white);
            p.drawText(pill, Qt::AlignCenter, label);
        }
    };
    const bool reorder = drag_.kind == Drag::Reorder && drag_.index < pr.pieces.size();
    for (int i = 0; i < pr.pieces.size(); ++i) {
        if (reorder && i == drag_.index) {  // Platzhalter, wo der Abschnitt herkommt
            p.setPen(QPen(th.border, 1.5, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(pieceRect(i), 7, 7);
            continue;
        }
        drawPiece(i, pieceRect(i));
    }
    if (reorder) {
        // Einfügestelle + der Abschnitt hängt an der Maus
        const int target = reorderTarget();
        double mx = LEFT;
        int seen = 0;
        for (int i = 0; i < pr.pieces.size(); ++i) {
            if (i == drag_.index) continue;
            if (seen++ < target) mx = pieceRect(i).right() + 1;
        }
        if (target == 0) {
            for (int i = 0; i < pr.pieces.size(); ++i)
                if (i != drag_.index) { mx = pieceRect(i).left() - 1; break; }
        }
        p.setPen(QPen(th.accent, 3));
        p.drawLine(QPointF(mx, RULER), QPointF(mx, RULER + ROW));
        p.setOpacity(0.85);
        drawPiece(drag_.index, pieceRect(drag_.index).translated(mouseX_ - drag_.x0, -6));
        p.setOpacity(1.0);
    }

    // Spuren: Hintergrund der Spur-Zeilen (auch leere Spuren sieht man), Ziel beim Verschieben hervorgehoben
    const QList<Row> rs = rows();
    for (const Row& row : rs) {
        if (row.kind == Row::Main || row.kind == Row::Lane) continue;
        QRectF band(0, row.y + 1, width(), row.h - 2);
        if (band.bottom() < vis.top() || band.top() > vis.bottom()) continue;
        QColor bg = mix(th.timeline, th.panel2, 0.55);
        if (dropTrack_ && row.id == dropTrack_) bg = mix(th.timeline, th.accent, 0.28);
        p.fillRect(band, bg);
    }
    if (dropTrack_ == kMainTrack) {
        p.setPen(QPen(th.accent, 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(1, RULER + 1, width() - 2, ROW - 4), 7, 7);
    }
    // Elemente (Bild/Text/Unschärfe/Video), dann Untertitel, dann Ton
    for (const Item& it : pr.items) {
        const QRectF r = itemRect(it, rs);
        if (r.isEmpty() || r.right() < vis.left() - 4 || r.left() > vis.right() + 4) continue;
        bool sel = host_->selItem() == it.id;
        QColor base = it.kind == Item::Blur ? blurColor() : it.kind == Item::Text ? textItemColor()
                      : it.kind == Item::Draw ? drawItemColor() : it.kind == Item::Shape ? shapeItemColor()
                      : it.kind == Item::Video ? videoClipColor() : imageColor();
        p.setBrush(sel ? base.lighter(125) : base);
        p.setPen(sel ? QPen(th.accent, 2) : QPen(Qt::NoPen));
        p.drawRoundedRect(r, 6, 6);
        p.save();
        p.setClipRect(r);
        if (it.kind == Item::Video && it.hasAudio)
            drawWave(p, r, host_->waves(it.path), it.srcStart, 1.0 / sc, it.volume, true, QColor(255, 255, 255, 70), vis);
        p.setPen(Qt::white);
        p.setFont(small);
        static const char* shapeKeys[] = {"shape_rect", "shape_ellipse", "shape_arrow", "shape_line"};
        QString name = it.kind == Item::Blur ? T("blur")
                       : it.kind == Item::Text ? it.text.section('\n', 0, 0)
                       : it.kind == Item::Draw ? T("drawing")
                       : it.kind == Item::Shape ? T(shapeKeys[std::clamp(it.shape, 0, 3)])
                                                : QFileInfo(it.path).fileName();
        if (it.media() && it.opacity < 0.995) name += QString("  ·  %1 %").arg(qRound(it.opacity * 100));
        p.drawText(r.adjusted(8, 0, -4, 0), Qt::AlignVCenter, name);
        p.restore();
    }
    for (int i = 0; i < pr.subs.size(); ++i) {
        const QRectF r = cueRect(i, rs);
        if (r.right() < vis.left() - 4 || r.left() > vis.right() + 4) continue;
        const bool sel = host_->selSub() == i;
        p.setBrush(sel ? subtitleColor().lighter(125) : subtitleColor());
        p.setPen(sel ? QPen(th.accent, 2) : QPen(QColor(0, 0, 0, 60), 1));
        p.drawRoundedRect(r, 5, 5);
        p.save();
        p.setClipRect(r.adjusted(2, 0, -2, 0));
        p.setPen(Qt::white);
        p.setFont(small);
        p.drawText(r.adjusted(6, 0, -3, 0), Qt::AlignVCenter, QString(pr.subs[i].text).replace('\n', ' '));
        p.restore();
    }
    for (int k = 0; k < pr.audios.size(); ++k) {
        const AudioClip& a = pr.audios[k];
        const QRectF r = audioRect(k);
        bool sel = host_->selAudio() == a.id;
        QColor base = audioColor();
        p.setBrush(sel ? base.lighter(118) : base.darker(115));
        p.setPen(sel ? QPen(th.accent, 2) : QPen(Qt::NoPen));
        p.drawRoundedRect(r, 6, 6);
        p.save();
        p.setClipRect(r);
        drawWave(p, r, host_->waves(a.path), a.srcStart, 1.0 / sc, a.volume, true, QColor(255, 255, 255, 120), vis);
        const double ly = volLineY(r, a.volume);
        const bool hot = (hover_ == Hover::Audio && hoverIndex_ == a.id) || (drag_.kind == Drag::AudioVol && drag_.index == a.id);
        p.setPen(QPen(QColor(255, 255, 255, hot ? 255 : 170), hot ? 2 : 1.3));
        p.drawLine(QPointF(r.left() + 2, ly), QPointF(r.right() - 2, ly));
        p.setPen(Qt::white);
        p.setFont(small);
        p.drawText(r.adjusted(8, 1, -4, 0), Qt::AlignTop | Qt::AlignLeft, QFileInfo(a.path).fileName());
        p.restore();
    }

    // dB-Anzeige an der Lautstärke-Linie
    if (drag_.kind == Drag::PieceVol && drag_.index < pr.pieces.size())
        drawDbPill(p, hoverX_, volLineY(waveRect(drag_.index), pr.pieces[drag_.index].volume), pr.pieces[drag_.index].volume);
    else if (drag_.kind == Drag::AudioVol || hover_ == Hover::Audio) {
        const int id = drag_.kind == Drag::AudioVol ? drag_.index : hoverIndex_;
        for (int k = 0; k < pr.audios.size(); ++k)
            if (pr.audios[k].id == id) drawDbPill(p, hoverX_, volLineY(audioRect(k), pr.audios[k].volume), pr.audios[k].volume);
    } else if (hover_ == Hover::Piece && hoverIndex_ >= 0 && hoverIndex_ < pr.pieces.size()) {
        drawDbPill(p, hoverX_, volLineY(waveRect(hoverIndex_), pr.pieces[hoverIndex_].volume), pr.pieces[hoverIndex_].volume);
    }

    // Hier landet die hineingezogene Datei
    if (dropX_ >= 0) {
        p.setPen(QPen(th.accent, 3, Qt::DashLine));
        p.drawLine(QPointF(dropX_, RULER), QPointF(dropX_, height()));
    }

    // Playhead (beim Kürzen am gezogenen Rand - dort zeigt die Vorschau das Bild)
    int x = int(frozen_ && drag_.kind == Drag::Trim ? trimEdgeX() : xOf(host_->curTime()));
    p.setPen(QPen(playheadColor(), 2));
    p.drawLine(x, 0, x, height());
    p.setPen(Qt::NoPen);
    p.setBrush(playheadColor());
    QPainterPath tri;
    tri.moveTo(x - 6, 0); tri.lineTo(x + 6, 0); tri.lineTo(x, 9); tri.closeSubpath();
    p.drawPath(tri);
}

Item* Timeline::itemAt(const QPointF& pos) {
    Project& pr = host_->pr();
    const QList<Row> rs = rows();
    // von hinten: was später eingefügt wurde, liegt bei gleicher Spur oben
    for (int k = pr.items.size() - 1; k >= 0; --k) {
        Item& it = pr.items[k];
        const QRectF r = itemRect(it, rs);
        if (!r.isEmpty() && pos.y() >= r.top() - 2 && pos.y() < r.bottom() + 2 && pos.x() >= r.left() - 4 && pos.x() <= r.right() + 4)
            return &it;
    }
    return nullptr;
}

AudioClip* Timeline::audioAt(const QPointF& pos, int* lane) {
    Project& pr = host_->pr();
    for (int k = pr.audios.size() - 1; k >= 0; --k) {
        AudioClip& a = pr.audios[k];
        const QRectF r = audioRect(k);
        if (!r.isEmpty() && pos.y() >= r.top() - 2 && pos.y() < r.bottom() + 2 && pos.x() >= r.left() - 4 && pos.x() <= r.right() + 4) {
            if (lane) *lane = k;
            return &a;
        }
    }
    return nullptr;
}

// Rand eines beliebigen Abschnitts unter der Maus (an der Grenze zweier Abschnitte zählt die Seite der Maus)
int Timeline::pieceEdgeAt(const QPointF& pos, char* side) const {
    Project& pr = host_->pr();
    if (pos.y() < RULER || pos.y() >= RULER + ROW) return -1;
    for (int i = 0; i < pr.pieces.size(); ++i) {
        const double x0 = xOf(pr.outStart(i)), x1 = xOf(pr.outStart(i) + pr.pieces[i].outDur());
        if (pos.x() >= x0 - 3 && pos.x() - x0 <= 7 && pos.x() < x1) { *side = 'l'; return i; }
        if (pos.x() <= x1 && x1 - pos.x() <= 7 && pos.x() > x0) { *side = 'r'; return i; }
    }
    return -1;
}

int Timeline::pieceVolAt(const QPointF& pos) const {
    Project& pr = host_->pr();
    for (int i = 0; i < pr.pieces.size(); ++i) {
        const QRectF rw = waveRect(i);
        if (pos.x() < rw.left() || pos.x() > rw.right()) continue;
        return std::abs(pos.y() - volLineY(rw, pr.pieces[i].volume)) <= 5 ? i : -1;
    }
    return -1;
}

AudioClip* Timeline::audioVolAt(const QPointF& pos) {
    int k = -1;
    AudioClip* a = audioAt(pos, &k);
    if (!a) return nullptr;
    const QRectF r = audioRect(k);
    return std::abs(pos.y() - volLineY(r, a->volume)) <= 5 ? a : nullptr;
}

void Timeline::mousePressEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    Project& pr = host_->pr();
    if (pr.pieces.isEmpty()) return;
    hoverX_ = mouseX_ = pos.x();

    // Lineal oder die Abspielmarke selbst: spulen (Ränder von Abschnitten und Elementen gehen aber vor -
    // nach dem Teilen liegt die Marke genau auf dem neuen Rand)
    const bool onPlayhead = std::abs(pos.x() - xOf(host_->curTime())) <= 4;
    auto seekDrag = [&] {
        drag_.kind = Drag::Seek;
        host_->seek(tOf(pos.x()));
    };
    if (pos.y() < RULER) return seekDrag();
    if (pos.y() < RULER + ROW) {
        char side = 0;
        const int edge = pieceEdgeAt(pos, &side);
        if (edge < 0 && onPlayhead) return seekDrag();
        if (edge >= 0) {
            const Piece& pc = pr.pieces[edge];
            host_->pushUndo();
            host_->selectPiece(edge);
            // Zeitleiste einfrieren: Maßstab und Lage der Abschnitte wie beim Anfassen
            frozenPps_ = pps();
            fx0_.clear();
            fx1_.clear();
            for (int k = 0; k < pr.pieces.size(); ++k) {
                fx0_ << xOf(pr.outStart(k));
                fx1_ << xOf(pr.outStart(k) + pr.pieces[k].outDur());
            }
            origOut_ = pc.outDur();
            frozen_ = true;
            drag_ = {Drag::Trim, edge, side, pos.x(), pc.start, pc.end, 0, pos.y()};
            host_->beginScrub();
            return;
        }
        const int vol = pieceVolAt(pos);
        if (vol >= 0) {
            host_->pushUndo();
            host_->selectPiece(vol);
            drag_ = {Drag::PieceVol, vol, 0, pos.x(), toDb(pr.pieces[vol].volume), 0, 0, pos.y()};
            update();
            return;
        }
        double t = tOf(pos.x());
        const int idx = pr.locate(t, nullptr);
        host_->selectPiece(idx);
        drag_ = {Drag::PiecePress, idx, 0, pos.x(), 0, 0, 0, pos.y()};
        host_->seek(t);
        return;
    }
    if (const int ci = cueAt(pos); ci >= 0) {
        host_->selectSub(ci);
        const SubCue& c = pr.subs[ci];
        const double x0 = xOf(c.t0), x1 = xOf(c.t1);
        char mode = std::abs(pos.x() - x0) <= 6 ? 'l' : std::abs(pos.x() - x1) <= 6 ? 'r' : 'm';
        host_->pushUndo();
        drag_ = {Drag::CueDrag, ci, mode, pos.x(), c.t0, c.t1, 0, pos.y()};
        return;
    }
    if (Item* it = itemAt(pos)) {
        host_->selectItem(it->id);
        double x0 = xOf(it->t0), x1 = xOf(it->t1);
        char mode = std::abs(pos.x() - x0) <= 6 ? 'l' : std::abs(pos.x() - x1) <= 6 ? 'r' : 'm';
        host_->pushUndo();
        drag_ = {Drag::ItemDrag, it->id, mode, pos.x(), it->t0, it->t1, it->srcStart, pos.y()};
    } else if (AudioClip* a = audioAt(pos)) {
        host_->selectAudio(a->id);
        double x0 = xOf(a->t0), x1 = xOf(a->t0 + a->dur);
        char mode = std::abs(pos.x() - x0) <= 6 ? 'l' : std::abs(pos.x() - x1) <= 6 ? 'r' : 'm';
        host_->pushUndo();
        if (mode == 'm' && audioVolAt(pos))
            drag_ = {Drag::AudioVol, a->id, 0, pos.x(), toDb(a->volume), 0, 0, pos.y()};
        else
            drag_ = {Drag::AudioDrag, a->id, mode, pos.x(), a->t0, a->dur, a->srcStart, pos.y()};
    } else if (onPlayhead) {
        seekDrag();
    } else {
        host_->selectItem(0);
        host_->selectAudio(0);
        host_->selectPiece(-1);
    }
}

void Timeline::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    Project& pr = host_->pr();
    hoverX_ = mouseX_ = pos.x();
    if (drag_.kind == Drag::None) {
        char side = 0;
        bool near = pieceEdgeAt(pos, &side) >= 0 || std::abs(pos.x() - xOf(host_->curTime())) <= 4;
        if (Item* it = itemAt(pos)) near |= std::abs(pos.x() - xOf(it->t0)) <= 6 || std::abs(pos.x() - xOf(it->t1)) <= 6;
        if (AudioClip* a = audioAt(pos)) near |= std::abs(pos.x() - xOf(a->t0)) <= 6 || std::abs(pos.x() - xOf(a->t0 + a->dur)) <= 6;
        Hover h = Hover::None;
        int idx = -1;
        if (!near) {
            if ((idx = pieceVolAt(pos)) >= 0) h = Hover::Piece;
            else if (AudioClip* a = audioVolAt(pos)) { h = Hover::Audio; idx = a->id; }
        }
        setCursor(near ? Qt::SizeHorCursor : h != Hover::None ? Qt::SizeVerCursor : Qt::ArrowCursor);
        if (h != hover_ || idx != hoverIndex_ || h != Hover::None) {
            hover_ = h;
            hoverIndex_ = idx;
            update();
        }
        return;
    }
    if (drag_.kind == Drag::PiecePress && std::abs(pos.x() - drag_.x0) > 6) {
        // Abschnitt anfassen und ziehen = verschieben (bei nur einem Abschnitt: spulen)
        if (pr.pieces.size() > 1) {
            drag_.kind = Drag::Reorder;
            setCursor(Qt::ClosedHandCursor);
        } else {
            drag_.kind = Drag::Seek;
        }
    }
    mouseY_ = pos.y();
    if (drag_.kind == Drag::Reorder) {
        // nach unten auf eine Video-Spur ziehen: Abschnitt wird dort zum Overlay
        const int tt = pos.y() >= RULER + ROW ? targetTrack(pos.y(), false) : 0;
        dropTrack_ = tt != kMainTrack ? tt : 0;
        update();
    } else if (drag_.kind == Drag::Seek) {
        host_->scrub(tOf(pos.x()));
    } else if (drag_.kind == Drag::Trim) {
        if (drag_.index >= pr.pieces.size()) return;
        Piece& pc = pr.pieces[drag_.index];
        double dt = (pos.x() - drag_.x0) / pps() * pc.speed;
        if (drag_.side == 'l') pc.start = std::clamp(drag_.a + dt, 0.0, pc.end - kMinPiece);
        else pc.end = std::clamp(drag_.b + dt, pc.start + kMinPiece, pr.srcDuration(drag_.index));
        host_->modelEdited(true);
        // Vorschau zeigt das Bild am gezogenen Rand
        const double t0 = pr.outStart(drag_.index);
        host_->scrub(drag_.side == 'l' ? t0 + 0.001 : std::max(t0, t0 + pc.outDur() - 0.04));
    } else if (drag_.kind == Drag::PieceVol || drag_.kind == Drag::AudioVol) {
        // nach oben lauter, nach unten leiser (feiner mit gedrückter Umschalttaste)
        const double perPx = (e->modifiers() & Qt::ShiftModifier) ? 0.08 : 0.35;
        const double v = fromDb(std::clamp(drag_.a - (pos.y() - drag_.y0) * perPx, -60.0, kMaxDb));
        if (drag_.kind == Drag::PieceVol) {
            if (drag_.index < pr.pieces.size()) pr.pieces[drag_.index].volume = v;
        } else if (AudioClip* a = pr.audio(drag_.index)) {
            a->volume = v;
        }
        host_->modelEdited(true);
    } else if (drag_.kind == Drag::CueDrag) {
        if (drag_.index >= pr.subs.size()) return;
        SubCue& c = pr.subs[drag_.index];
        const double dt = (pos.x() - drag_.x0) / pps(), total = pr.total();
        if (drag_.side == 'm') {
            const double dur = drag_.b - drag_.a;
            c.t0 = std::clamp(drag_.a + dt, 0.0, std::max(0.0, total - dur));
            c.t1 = c.t0 + dur;
        } else if (drag_.side == 'l') {
            c.t0 = std::clamp(drag_.a + dt, 0.0, c.t1 - 0.1);
        } else {
            c.t1 = std::clamp(drag_.b + dt, c.t0 + 0.1, total);
        }
        host_->modelEdited(true);
    } else if (drag_.kind == Drag::ItemDrag) {
        Item* it = pr.item(drag_.index);
        if (!it) return;
        double dt = (pos.x() - drag_.x0) / pps(), total = pr.total();
        // Bild/Video senkrecht auf eine andere Video-Spur ziehen (V1 erst beim Loslassen)
        if (it->media() && drag_.side == 'm' && std::abs(pos.y() - drag_.y0) > 8) {
            const int tt = targetTrack(pos.y(), false);
            dropTrack_ = tt == kMainTrack && it->kind == Item::Video ? kMainTrack : 0;
            if (tt && tt != kMainTrack && tt != it->track) {
                it->track = tt;
                host_->modelEdited(true);
                relayout();
            }
        }
        if (it->kind == Item::Video && drag_.side == 'l') {
            // linken Rand eines Videos kürzen: Startpunkt in der Datei wandert mit
            const double d = std::clamp(dt, -std::min(drag_.a, drag_.c), drag_.b - drag_.a - 0.1);
            it->t0 = drag_.a + d;
            it->srcStart = drag_.c + d;
            host_->modelEdited(true);
            return;
        }
        if (it->kind == Item::Video && drag_.side == 'r') {
            it->t1 = std::clamp(drag_.b + dt, it->t0 + 0.1, std::min(total, it->t0 + it->fileDur - it->srcStart));
            host_->modelEdited(true);
            return;
        }
        if (drag_.side == 'm') {
            double dur = drag_.b - drag_.a;
            it->t0 = std::clamp(drag_.a + dt, 0.0, std::max(0.0, total - dur));
            it->t1 = it->t0 + dur;
        } else if (drag_.side == 'l') {
            it->t0 = std::clamp(drag_.a + dt, 0.0, it->t1 - 0.1);
        } else {
            it->t1 = std::clamp(drag_.b + dt, it->t0 + 0.1, total);
        }
        host_->modelEdited(true);
    } else if (drag_.kind == Drag::AudioDrag) {
        AudioClip* a = pr.audio(drag_.index);
        if (!a) return;
        double dt = (pos.x() - drag_.x0) / pps();
        if (drag_.side == 'm') {
            a->t0 = std::max(0.0, drag_.a + dt);
            if (std::abs(pos.y() - drag_.y0) > 8)
                if (const int tt = targetTrack(pos.y(), true); tt && tt != pr.audioTrackOf(*a)) a->track = tt;
        } else if (drag_.side == 'l') {
            double d = std::clamp(dt, -std::min(drag_.a, drag_.c), drag_.b - 0.1);
            a->t0 = drag_.a + d;
            a->srcStart = drag_.c + d;
            a->dur = drag_.b - d;
        } else {
            a->dur = std::clamp(drag_.b + dt, 0.1, a->fileDur - a->srcStart);
        }
        host_->modelEdited(true);
    }
}

void Timeline::mouseReleaseEvent(QMouseEvent*) {
    Project& pr = host_->pr();
    const Drag d = drag_;
    drag_.kind = Drag::None;
    const int dropTo = dropTrack_;
    dropTrack_ = 0;
    if (d.kind == Drag::Reorder && d.index < pr.pieces.size() && dropTo) {
        unsetCursor();
        host_->moveToTrack('p', d.index, dropTo);
        relayout();
        return;
    }
    if (d.kind == Drag::ItemDrag && dropTo == kMainTrack) {
        host_->moveToTrack('i', d.index, kMainTrack);
        relayout();
        return;
    }
    if (d.kind == Drag::ItemDrag || d.kind == Drag::AudioDrag || d.kind == Drag::CueDrag) host_->modelEdited(false);
    if (d.kind == Drag::Reorder && d.index < pr.pieces.size()) {
        const int target = reorderTarget();
        unsetCursor();
        if (target != d.index) {
            host_->pushUndo();
            pr.pieces.move(d.index, target);
            host_->selectPiece(target);
            host_->modelEdited(true);
            host_->seek(pr.outStart(target) + 0.001);
        }
        relayout();
        return;
    }
    if (d.kind == Drag::Trim) {
        frozen_ = false;  // Lücke schließen: das Hauptvideo rückt wieder zusammen
        relayout();
    }
    if (d.kind == Drag::Trim || d.kind == Drag::Seek) {
        host_->seek(host_->curTime());
        host_->endScrub();
    }
    update();
}

void Timeline::leaveEvent(QEvent*) {
    if (hover_ != Hover::None) {
        hover_ = Hover::None;
        update();
    }
}

void Timeline::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        zoomBy(e->angleDelta().y() > 0 ? 1.25 : 1 / 1.25);
        e->accept();
    } else {
        e->ignore();
    }
}

// ---------------------------------------------------------------- Drag & Drop
void Timeline::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void Timeline::dragMoveEvent(QDragMoveEvent* e) {
    if (!e->mimeData()->hasUrls()) return;
    e->acceptProposedAction();
    const double total = host_->pr().total();
    dropX_ = xOf(std::clamp(tOf(e->position().x()), 0.0, total));
    update();
}

void Timeline::dragLeaveEvent(QDragLeaveEvent*) {
    dropX_ = -1;
    update();
}

void Timeline::dropEvent(QDropEvent* e) {
    dropX_ = -1;
    update();
    const double t = std::clamp(tOf(e->position().x()), 0.0, host_->pr().total());
    // auf eine Spur gezogen: dort einfügen (Video-Spur = Bild-im-Bild, Ton-Spur = Ton), sonst wie bisher
    const QList<Row> rs = rows();
    const int ri = rowAt(e->position().y(), rs);
    const int track = ri >= 0 && (rs[ri].kind == Row::VTrack || rs[ri].kind == Row::ATrack) ? rs[ri].id : 0;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        if (track) host_->insertMediaOnTrack(u.toLocalFile(), t, track);
        else host_->insertMediaAt(u.toLocalFile(), t);
    }
    e->acceptProposedAction();
}

// Rechtsklick auf einen Clip: auf eine andere Spur legen
void Timeline::contextMenuEvent(QContextMenuEvent* e) {
    Project& pr = host_->pr();
    const QPointF pos = e->pos();
    char kind = 0;
    int id = 0;
    bool audio = false, toMain = false;
    if (pos.y() >= RULER && pos.y() < RULER + ROW && !pr.pieces.isEmpty()) {
        kind = 'p';
        id = pr.locate(tOf(pos.x()), nullptr);
        host_->selectPiece(id);
    } else if (Item* it = itemAt(pos); it && it->media()) {
        kind = 'i';
        id = it->id;
        toMain = it->kind == Item::Video;
        host_->selectItem(id);
    } else if (AudioClip* a = audioAt(pos)) {
        kind = 'a';
        id = a->id;
        audio = true;
        host_->selectAudio(id);
    }
    if (!kind) return;
    QMenu m(this);
    QMenu* sub = m.addMenu(T("move_to_track"));
    const int cur = kind == 'p' ? kMainTrack : kind == 'i' ? pr.item(id)->track : pr.audioTrackOf(*pr.audio(id));
    for (int tid : pr.trackIds(audio)) {
        if (tid == kMainTrack && !toMain) continue;
        QAction* act = sub->addAction(pr.trackName(tid) + (tid == kMainTrack ? "  ·  " + T("main_video") : QString()),
                                      this, [this, kind, id, tid] { host_->moveToTrack(kind, id, tid); });
        act->setEnabled(tid != cur);
    }
    sub->addSeparator();
    sub->addAction(audio ? T("new_audio_track") : T("new_video_track"), this, [this, kind, id] { host_->moveToTrack(kind, id, -1); });
    if (kind == 'p' && (pr.pieces.size() < 2 || std::abs(pr.pieces[id].speed - 1.0) > 1e-6 || !pr.sources.value(pr.pieces[id].src).video))
        sub->setEnabled(false);  // letzter Abschnitt, anderes Tempo oder Tondatei: bleibt im Hauptvideo
    m.exec(e->globalPos());
}

// ---------------------------------------------------------------- Spurköpfe
TrackHeader::TrackHeader(EditorHost* host, Timeline* tl, QScrollArea* scroll) : host_(host), tl_(tl), scroll_(scroll) {
    setFixedWidth(W);
    setMouseTracking(true);
    tl_->setHeader(this);
    connect(scroll_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { update(); });
}

double TrackHeader::offset() const {
    // Zeitleiste und Köpfe stehen oben bündig: gleiche Höhe wie der Rahmen der Bildlauf-Fläche
    return scroll_->viewport()->mapTo(scroll_, QPoint(0, 0)).y() - scroll_->verticalScrollBar()->value();
}

QList<TrackHeader::Hit> TrackHeader::hits() const {
    QList<Hit> out;
    const double off = offset();
    out << Hit{0, '+', QRectF(W - 30, off + 2, 24, 22)};
    for (const Timeline::Row& r : tl_->rows()) {
        if (r.kind == Timeline::Row::Lane || r.kind == Timeline::Row::Subs) continue;
        const double cy = off + r.y + (r.kind == Timeline::Row::Main ? 24 : r.h / 2);
        out << Hit{r.id, 'n', QRectF(4, off + r.y + 1, W - 8, r.h - 2)};
        out << Hit{r.id, 'm', QRectF(34, cy - 10, 20, 20)};
        out << Hit{r.id, 's', QRectF(57, cy - 10, 20, 20)};
        out << Hit{r.id, 'v', QRectF(82, cy - 5, W - 92, 10)};
    }
    return out;
}

TrackHeader::Hit TrackHeader::hitAt(const QPointF& p) const {
    Hit best;
    for (const Hit& h : hits())
        if (h.r.adjusted(-2, -3, 2, 3).contains(p) && (best.what == 0 || best.what == 'n')) best = h;
    return best;
}

void TrackHeader::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Theme& th = currentTheme();
    Project& pr = host_->pr();
    const double off = offset();
    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    p.save();
    p.setClipRect(QRectF(0, scroll_->viewport()->mapTo(scroll_, QPoint(0, 0)).y(), W, scroll_->viewport()->height()));
    bool anySolo = false;
    for (const Track& t : pr.tracks) anySolo |= t.solo;
    for (const Timeline::Row& r : tl_->rows()) {
        const QRectF row(2, off + r.y + 1, W - 4, r.h - 2);
        if (r.kind == Timeline::Row::Lane || r.kind == Timeline::Row::Subs) {
            if (r.kind == Timeline::Row::Subs) {
                p.setPen(th.muted);
                p.drawText(row.adjusted(8, 0, 0, 0), Qt::AlignVCenter, T("subtitles"));
            }
            continue;
        }
        const Track* t = pr.track(r.id);
        if (!t) continue;
        const bool sel = host_->selTrack() == r.id;
        p.setPen(sel ? QPen(th.accent, 1.5) : Qt::NoPen);
        p.setBrush(sel ? mix(th.panel2, th.accent, 0.18) : th.panel2);
        p.drawRoundedRect(row, 6, 6);
        const double cy = off + r.y + (r.kind == Timeline::Row::Main ? 24 : r.h / 2);
        const bool silent = t->mute || (anySolo && !t->solo);
        p.setPen(silent ? th.muted : th.text);
        p.drawText(QRectF(8, cy - 10, 26, 20), Qt::AlignVCenter, pr.trackName(r.id));
        auto toggle = [&](double x, const QString& txt, bool on) {
            const QRectF b(x, cy - 10, 20, 20);
            p.setPen(on ? Qt::NoPen : QPen(th.border, 1));
            p.setBrush(on ? th.accent : Qt::transparent);
            p.drawRoundedRect(b, 4, 4);
            p.setPen(on ? th.accentText : th.muted);
            p.drawText(b, Qt::AlignCenter, txt);
        };
        toggle(34, "M", t->mute);
        toggle(57, "S", t->solo);
        // Lautstärke als kleiner Balken (0 … 200 %), Doppelklick = 100 %
        const QRectF vb(82, cy - 3, W - 92, 6);
        p.setPen(Qt::NoPen);
        p.setBrush(th.border);
        p.drawRoundedRect(vb, 3, 3);
        const double frac = std::clamp(t->volume / 2.0, 0.0, 1.0);
        p.setBrush(silent ? th.muted : th.accent);
        p.drawRoundedRect(QRectF(vb.left(), vb.top(), std::max(6.0, vb.width() * frac), vb.height()), 3, 3);
        p.setPen(QPen(th.text, 1));
        p.drawLine(QPointF(vb.left() + vb.width() / 2, vb.top() - 2), QPointF(vb.left() + vb.width() / 2, vb.bottom() + 2));
        if (r.kind == Timeline::Row::Main) {
            QFont sf = f;
            sf.setBold(false);
            p.setFont(sf);
            p.setPen(th.muted);
            p.drawText(QRectF(8, cy + 12, W - 16, 16), Qt::AlignVCenter, T("main_video"));
            p.drawText(QRectF(8, cy + 28, W - 16, 16), Qt::AlignVCenter, QString("%1 %").arg(qRound(t->volume * 100)));
            p.setFont(f);
        }
    }
    p.restore();
    // "+": Spur anlegen
    const QRectF plus(W - 30, off + 2, 24, 22);
    if (plus.bottom() > 0) {
        p.setPen(QPen(th.border, 1));
        p.setBrush(th.panel2);
        p.drawRoundedRect(plus, 5, 5);
        p.drawPixmap(plus.adjusted(5, 4, -5, -4).toRect(), makeIcon(Ic::Plus, th.text).pixmap(28, 28));
    }
}

void TrackHeader::mousePressEvent(QMouseEvent* e) {
    const Hit h = hitAt(e->position());
    Project& pr = host_->pr();
    if (h.what == '+') {
        QMenu m(this);
        m.addAction(makeIcon(Ic::Layers, currentTheme().text), T("new_video_track"), this, [this] {
            host_->pushUndo();
            host_->selectTrack(host_->pr().addTrack(false));
            host_->modelEdited(true);
        });
        m.addAction(makeIcon(Ic::Music, currentTheme().text), T("new_audio_track"), this, [this] {
            host_->pushUndo();
            host_->selectTrack(host_->pr().addTrack(true));
            host_->modelEdited(true);
        });
        m.exec(mapToGlobal(h.r.bottomLeft().toPoint()));
        return;
    }
    Track* t = pr.track(h.track);
    if (!t) return;
    if (h.what == 'm' || h.what == 's') {
        host_->pushUndo();
        (h.what == 'm' ? t->mute : t->solo) ^= true;
        host_->tracksChanged();
    } else if (h.what == 'v') {
        host_->pushUndo();
        volDrag_ = h.track;
        mouseMoveEvent(e);
    } else {
        host_->selectTrack(h.track);
    }
    update();
}

void TrackHeader::mouseMoveEvent(QMouseEvent* e) {
    if (volDrag_) {
        for (const Hit& h : hits())
            if (h.track == volDrag_ && h.what == 'v') {
                double v = std::clamp((e->position().x() - h.r.left()) / h.r.width(), 0.0, 1.0) * 2.0;
                if (std::abs(v - 1.0) < 0.04) v = 1.0;  // rastet bei 100 % ein
                if (Track* t = host_->pr().track(volDrag_)) t->volume = std::round(v * 100) / 100;
                host_->tracksChanged();
                setToolTip(QString("%1 %").arg(qRound(host_->pr().track(volDrag_)->volume * 100)));
            }
        update();
        return;
    }
    const Hit h = hitAt(e->position());
    const Track* t = host_->pr().track(h.track);
    setToolTip(h.what == 'm' ? T("track_mute") : h.what == 's' ? T("track_solo") : h.what == '+' ? T("add_track")
               : h.what == 'v' && t ? T("volume") + QString(" %1 %").arg(qRound(t->volume * 100)) : QString());
    setCursor(h.what && h.what != 'n' ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void TrackHeader::mouseReleaseEvent(QMouseEvent*) { volDrag_ = 0; }

void TrackHeader::mouseDoubleClickEvent(QMouseEvent* e) {
    const Hit h = hitAt(e->position());
    if (h.what != 'v') return;
    if (Track* t = host_->pr().track(h.track)) {
        t->volume = 1.0;
        host_->tracksChanged();
        update();
    }
}
