#pragma once
#include <QGraphicsView>
#include <QImage>
#include <QPixmap>

// Videoansicht: weiches "Ambient"-Licht in den freien Rändern, Klick auf leeres Video = Pause/Play.
// Mit aktivem Werkzeug (Zeichnen / Form) gehen alle Mausbewegungen an das Werkzeug statt an die Elemente.
class VideoView : public QGraphicsView {
    Q_OBJECT
public:
    explicit VideoView(QGraphicsScene* scene, QWidget* parent = nullptr);
    void setAmbient(const QImage& tiny);
    void setToolActive(bool on);
    // Reine Tondatei (wie ein Musik-Player): Cover groß und verschwommen im Hintergrund, klein unten links,
    // daneben Titel und "Interpret • Album". Ohne Cover: gezeichnete Noten. Leerer Titel = normales Video.
    void setAudioInfo(const QString& title, const QString& subtitle = QString(), const QImage& cover = QImage());

signals:
    void emptyClicked();
    void mouseActivity();
    void toolPressed(const QPointF& scenePos);
    void toolMoved(const QPointF& scenePos);
    void toolReleased(const QPointF& scenePos);

protected:
    void drawBackground(QPainter* p, const QRectF& r) override;
    void drawForeground(QPainter* p, const QRectF& r) override;
    void paintBliss(QPainter* p, const QRect& r);
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    QImage ambient_;
    QString audioTitle_, audioSub_;
    QImage cover_, coverBlur_;
    QPixmap smoke_;  // Acryl: Hintergrund passend zur Größe, damit das Video flüssig bleibt
    QRgb smokeKey_ = 0;
    bool pressOnEmpty_ = false, tool_ = false, toolDown_ = false;
    QPoint pressPos_;
};
