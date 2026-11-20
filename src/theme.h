#pragma once
#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

struct Theme {
    QString id, name;
    QColor bg, panel, panel2, border, text, muted, accent, accentText, timeline, video;
    QColor hover;      // Hintergrund beim Überfahren (YouTube-Stil)
    QString font;
    int fontPx = 13;
    int radius = 8;
    bool xp = false;
    bool yt = false;   // Stil Minimal: runde Knöpfe ohne Rahmen; ausgewählt = Textfarbe als Fläche
    bool acryl = false;  // Stil Acryl: rauchiger Hintergrund, Flächen wie mattes Glas
};

const QList<Theme>& themes();
const Theme& currentTheme();
void setCurrentTheme(const QString& id);
// Eigene Akzentfarbe (Knöpfe, Fortschritt, Auswahl); leer = Farbe des Designs. Windows XP behält seine Farben.
void setAccentColor(const QString& color);
QString accentColor();
const QList<QColor>& accentChoices();
QString buildStyleSheet(const Theme& t);
// Weichzeichnen: dreimal Kastenfilter in beide Richtungen, Radius r in Pixeln
QImage softBlur(const QImage& src, int r);
// Rauchiger Hintergrund des Stils Acryl, in der Farbfamilie der Akzentfarbe
const QImage& acrylSmoke(const Theme& t);

// Feste Farben für Timeline-Elemente (in jedem Theme gut lesbar)
// kein Orange: steht in der Zeitleiste direkt neben dem Grün von Zeichnungen und grünen Designs
inline QColor blurColor() { return QColor("#7c8798"); }
inline QColor imageColor() { return QColor("#4a90e2"); }
inline QColor textItemColor() { return QColor("#d9488f"); }
inline QColor audioColor() { return QColor("#9b6be0"); }
inline QColor videoClipColor() { return QColor("#2f6fb0"); }  // Video auf einer Overlay-Spur
inline QColor subtitleColor() { return QColor("#5c6bc0"); }
inline QColor drawItemColor() { return QColor("#2bb673"); }
inline QColor shapeItemColor() { return QColor("#14a3a8"); }
inline QColor playheadColor() { return QColor("#ff5d5d"); }
