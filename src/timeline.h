#pragma once
#include <QWidget>
#include "host.h"

class QScrollArea;

class Timeline : public QWidget {
public:
    // Zeilen der Zeitleiste von oben nach unten: V1 (Hauptvideo), weitere Video-Spuren, eigene Zeilen für Texte,
    // Unschärfe, Zeichnungen …, Untertitel, Ton-Spuren
    struct Row {
        enum Kind { Main, VTrack, Lane, Subs, ATrack } kind;
        int id = 0;  // Spur-ID (Main/VTrack/ATrack) bzw. Element-ID (Lane)
        double y = 0, h = 0;
    };
    explicit Timeline(EditorHost* host);
    QList<Row> rows() const;
    int rowAt(double y, const QList<Row>& rows) const;
    void setHeader(QWidget* h) { header_ = h; }
    void relayout();
    void zoomBy(double factor);
    double playheadX() const { return xOf(host_->curTime()); }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    // Medien aus dem Medien-Panel (oder dem Explorer) an eine Stelle ziehen
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dragLeaveEvent(QDragLeaveEvent*) override;
    void dropEvent(QDropEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    static constexpr int RULER = 26, ROW = 86, WAVE = 26, LANE = 32, LEFT = 14;
    double basePps() const;
    double pps() const;
    double xOf(double t) const { return LEFT + t * pps(); }
    double tOf(double x) const { return std::max(0.0, (x - LEFT) / pps()); }
    int laneY(int lane) const { return RULER + ROW + 6 + lane * LANE; }
    QRectF pieceRect(int i) const;
    QRectF waveRect(int i) const;  // Ton-Streifen unter den Vorschaubildern
    QRectF audioRect(int k) const;
    QRectF itemRect(const Item& it, const QList<Row>& rows) const;
    QRectF cueRect(int i, const QList<Row>& rows) const;
    int cueAt(const QPointF& pos) const;
    int targetTrack(double y, bool audio) const;  // Spur unter der Maus beim Verschieben (0 = keine)
    double volLineY(const QRectF& area, double volume) const;
    Item* itemAt(const QPointF& pos);
    AudioClip* audioAt(const QPointF& pos, int* lane = nullptr);
    int pieceEdgeAt(const QPointF& pos, char* side) const;
    int pieceVolAt(const QPointF& pos) const;
    AudioClip* audioVolAt(const QPointF& pos);
    void drawWave(QPainter& p, const QRectF& area, const WaveSet* ws, double srcAtLeft, double srcPerPx,
                  double volume, bool centered, const QColor& color, const QRect& vis) const;
    void drawDbPill(QPainter& p, double x, double y, double volume) const;

    struct Drag {
        // PiecePress: in einen Abschnitt geklickt - wird beim Ziehen zu Reorder (Abschnitt verschieben) bzw. Seek
        enum Kind { None, Seek, Trim, ItemDrag, AudioDrag, PieceVol, AudioVol, PiecePress, Reorder, CueDrag } kind = None;
        int index = 0;      // Abschnitt-Index bzw. ID
        char side = 0;      // 'l', 'r', 'm'
        double x0 = 0, a = 0, b = 0, c = 0, y0 = 0;
    } drag_;
    // Beim Kürzen bleibt die Zeitleiste stehen: der gezogene Rand folgt der Maus, die anderen Abschnitte bleiben,
    // wo sie waren. Erst beim Loslassen rückt alles wieder zusammen.
    bool frozen_ = false;
    double frozenPps_ = 0, origOut_ = 0;
    QList<double> fx0_, fx1_;
    double trimEdgeX() const;
    int reorderTarget() const;  // Einfügestelle beim Verschieben eines Abschnitts
    double mouseX_ = 0;
    // Lautstärke-Linie unter der Maus (für die dB-Anzeige)
    enum class Hover { None, Piece, Audio } hover_ = Hover::None;
    int hoverIndex_ = -1;
    double hoverX_ = 0;
    double dropX_ = -1;  // Markierung beim Hineinziehen

    EditorHost* host_;
    QWidget* header_ = nullptr;
    double zoom_ = 1.0;
    int dropTrack_ = 0;  // beim Ziehen eines Abschnitts/Clips: Ziel-Spur (Hervorhebung)
    double mouseY_ = 0;
};

// Köpfe der Spuren links neben der Zeitleiste: Name, Stumm (M), Solo (S), Lautstärke; "+" legt Spuren an.
// Scrollt senkrecht mit der Zeitleiste.
class TrackHeader : public QWidget {
public:
    TrackHeader(EditorHost* host, Timeline* tl, QScrollArea* scroll);
    static constexpr int W = 128;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    struct Hit { int track = 0; char what = 0; QRectF r; };  // what: 'n' Name, 'm' Stumm, 's' Solo, 'v' Lautstärke, '+'
    QList<Hit> hits() const;
    Hit hitAt(const QPointF& p) const;
    double offset() const;
    EditorHost* host_;
    Timeline* tl_;
    QScrollArea* scroll_;
    int volDrag_ = 0;
};
