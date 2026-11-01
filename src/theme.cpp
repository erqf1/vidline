#include "theme.h"
#include <QPixmap>
#include <QPainter>
#include <QFile>
#include <QDir>

static QList<Theme> makeThemes() {
    auto mk = [](const char* id, const char* name, const char* bg, const char* panel, const char* panel2,
                 const char* border, const char* text, const char* muted, const char* accent, const char* acctext,
                 const char* tl, const char* video, const char* font = "'Segoe UI','SF Pro Text','Helvetica Neue','Ubuntu','Noto Sans','DejaVu Sans',sans-serif", int px = 13, int r = 8,
                 bool xp = false) {
        Theme t;
        t.id = id; t.name = name;
        t.bg = bg; t.panel = panel; t.panel2 = panel2; t.border = border; t.text = text; t.muted = muted;
        t.accent = accent; t.accentText = acctext; t.timeline = tl; t.video = video;
        t.font = font ? font : "'Segoe UI','SF Pro Text','Helvetica Neue','Ubuntu','Noto Sans','DejaVu Sans',sans-serif";
        t.fontPx = px; t.radius = r; t.xp = xp;
        return t;
    };
    // Minimal: ruhige neutrale Grautöne, runde Knöpfe ohne Rahmen, keine knallige Farbe (Akzent frei wählbar)
    auto minimal = [&](const char* id, const char* name, const char* bg, const char* panel, const char* panel2,
                       const char* border, const char* text, const char* muted, const char* accent, const char* acctext,
                       const char* tl, const char* hover) {
        Theme t = mk(id, name, bg, panel, panel2, border, text, muted, accent, acctext, tl, "#000000", nullptr, 13, 12);
        t.hover = QColor(hover);
        t.yt = true;
        return t;
    };
    QList<Theme> list = {
        mk("mint", "Mint", "#0d0f14", "#151821", "#1c202b", "#2a2f3d", "#e6e8ee", "#8b90a0", "#4ade9b", "#04231a",
           "#0a0c11", "#000000"),
        // Studio: schlicht, dunkelgrau mit Türkis – wie bekannte Schnittprogramme
        mk("studio", "Studio", "#161616", "#202020", "#2a2a2a", "#333333", "#e8e8e8", "#8c8c8c", "#1fc8d4", "#062326",
           "#1a1a1a", "#000000", nullptr, 12, 6),
        mk("ocean", "Ocean", "#0a101d", "#101a2e", "#16233c", "#263557", "#e3ecff", "#8fa3c7", "#4aa3ff", "#04101f",
           "#08101c", "#000000"),
        mk("violet", "Violett", "#100c1b", "#181227", "#221a36", "#392c59", "#efe9ff", "#9d8fc0", "#a78bfa", "#140a2e",
           "#0c0916", "#000000"),
        mk("sunset", "Sunset", "#17100e", "#221815", "#2d211c", "#493429", "#fbeee6", "#b59a8a", "#ff8a4c", "#2a1204",
           "#120c0a", "#000000"),
        mk("light", "Hell / Light", "#eef0f5", "#ffffff", "#eceef4", "#d3d7e0", "#1b1f2a", "#6a7083", "#2a9d6f",
           "#ffffff", "#e4e7ee", "#111111"),
        minimal("minimal", "Minimal", "#111111", "#1a1a1a", "#262626", "#2e2e2e", "#ededed", "#9a9a9a", "#ededed",
                "#111111", "#141414", "#383838"),
        minimal("minimal_light", "Minimal hell / light", "#fafafa", "#ffffff", "#f0f0f0", "#e4e4e4", "#161616", "#6b6b6b",
                "#161616", "#ffffff", "#f0f0f0", "#e4e4e4"),
        mk("xp", "Windows XP", "#ECE9D8", "#F7F6EE", "#FFFFFF", "#7F9DB9", "#000000", "#4d4d4d", "#316AC5", "#FFFFFF",
           "#FFFFFF", "#000000", "'Tahoma','Verdana','DejaVu Sans',sans-serif", 12, 3, true),
        mk("terminal", "Terminal", "#0C0C0C", "#121212", "#1A1A1A", "#3A3A3A", "#CCCCCC", "#767676", "#F2F2F2", "#0C0C0C",
           "#080808", "#000000", "'Cascadia Mono','Consolas','Menlo','DejaVu Sans Mono',monospace", 13, 0),
    };
    for (Theme& t : list)
        if (!t.hover.isValid()) t.hover = t.border;
    return list;
}

const QList<Theme>& themes() {
    static const QList<Theme> t = makeThemes();
    return t;
}

static QString g_id = "mint", g_accent;
static Theme g_current;  // Design mit eigener Akzentfarbe

static void rebuildCurrent() {
    g_current = themes().first();
    for (const Theme& t : themes())
        if (t.id == g_id) g_current = t;
    const QColor c(g_accent);
    if (c.isValid() && !g_current.xp) {
        g_current.accent = c;
        // Schrift auf der Akzentfläche: dunkel auf hellen Farben, sonst weiß
        g_current.accentText = c.lightnessF() > 0.62 ? QColor("#0f0f0f") : QColor("#ffffff");
    }
}

const Theme& currentTheme() {
    if (g_current.id.isEmpty()) rebuildCurrent();
    return g_current;
}

void setCurrentTheme(const QString& id) {
    g_id = id;
    rebuildCurrent();
}

void setAccentColor(const QString& color) {
    g_accent = QColor(color).isValid() ? QColor(color).name() : QString();
    rebuildCurrent();
}

QString accentColor() { return g_accent; }

const QList<QColor>& accentChoices() {
    static const QList<QColor> c = {QColor("#4ade9b"), QColor("#ff4d6d"), QColor("#ff7a3d"), QColor("#f5b400"),
                                    QColor("#22c55e"), QColor("#14b8a6"), QColor("#3ea6ff"), QColor("#8b5cf6"),
                                    QColor("#ec4899")};
    return c;
}

// Grüner XP-Haken für Checkboxen (einmal gezeichnet, als Datei für das Stylesheet)
static QString xpCheckImage() {
    const QString path = QDir::temp().filePath("vidline-xp-check.png");
    if (!QFile::exists(path)) {
        QPixmap pm(26, 26);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#21A121"), 4.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(6, 13.5), QPointF(11, 18.5), QPointF(20, 7.5)}));
        p.end();
        pm.save(path);
    }
    return QDir::fromNativeSeparators(path);
}

QString buildStyleSheet(const Theme& t) {
    QString s = R"(
* { font-family: @font@; font-size: @fs@px; color: @text@; }
QMainWindow, QDialog, QWidget#root { background: @bg@; }
QLabel { background: transparent; }
QLabel#hint { color: @muted@; }
QLabel#title { font-size: @fs2@px; font-weight: 600; }
QFrame#card { background: @panel@; border: 1px solid @border@; border-radius: @r@px; }
QFrame#card QLabel { background: transparent; }
QPushButton { background: @panel2@; border: 1px solid @border@; border-radius: @r@px; padding: 7px 12px; }
QPushButton:hover { border-color: @accent@; }
QPushButton:pressed { background: @border@; }
QPushButton:checked { background: @accent@; color: @acctext@; border-color: @accent@; }
QPushButton:disabled { color: @muted@; }
QPushButton#primary { background: @accent@; color: @acctext@; border-color: @accent@; font-weight: 600; }
QPushButton#primary:hover { background: @accent2@; }
QPushButton#play { border-radius: 20px; min-width: 40px; max-width: 40px; min-height: 40px; max-height: 40px;
                   padding: 0; background: @accent@; border-color: @accent@; }
QPushButton#play:hover { background: @accent2@; }
QPushButton#tool { padding: 6px 10px; }
QPushButton#paneHeader { background: transparent; border: none; padding: 2px 0; text-align: left; font-weight: 600; font-size: 14px; }
QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit { background: @panel2@; border: 1px solid @border@;
    border-radius: @r6@px; padding: 5px 8px; selection-background-color: @accent@; selection-color: @acctext@; }
QComboBox::drop-down { border: none; width: 22px; }
QPlainTextEdit { background: @panel2@; border: 1px solid @border@; border-radius: @r6@px; padding: 4px 6px;
    selection-background-color: @accent@; selection-color: @acctext@; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @border@; outline: 0;
    selection-background-color: @accent@; selection-color: @acctext@; }
QListWidget { background: @panel2@; border: 1px solid @border@; border-radius: @r@px; padding: 4px; outline: 0; }
QListWidget::item { padding: 9px 10px; border-radius: @r6@px; }
QListWidget::item:hover { background: @border@; }
QListWidget::item:selected { background: @accent@; color: @acctext@; }
QGraphicsView { border: none; border-radius: @r@px; background: @video@; }
QScrollArea { border: 1px solid @border@; border-radius: @r@px; background: @tl@; }
QScrollBar:horizontal { height: 10px; background: transparent; margin: 0; }
QScrollBar::handle:horizontal { background: @border@; border-radius: 5px; min-width: 30px; }
QScrollBar:vertical { width: 10px; background: transparent; margin: 0; }
QScrollBar::handle:vertical { background: @border@; border-radius: 5px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: @border@; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -6px 0; background: @text@; border-radius: 7px; }
QToolTip { background: @panel@; color: @text@; border: 1px solid @border@; padding: 4px; }
QProgressBar { background: @panel2@; border: 1px solid @border@; border-radius: @r6@px; text-align: center; height: 16px; }
QProgressBar::chunk { background: @accent@; border-radius: @r6@px; }
QSplitter::handle { background: transparent; }
QSplitter::handle:hover { background: @accent@; border-radius: 2px; }
)";
    if (t.yt) {
        // Minimal: runde Knöpfe ohne Rahmen, ausgewählt = Textfarbe als Fläche
        s += R"(
QPushButton { background: @panel2@; border: none; border-radius: 18px; padding: 8px 16px; font-weight: 500; }
QPushButton:hover { background: @hover@; }
QPushButton:pressed { background: @border@; }
QPushButton:checked { background: @text@; color: @bg@; }
QPushButton:checked:hover { background: @muted@; }
QPushButton#primary { background: @accent@; color: @acctext@; font-weight: 600; }
QPushButton#primary:hover { background: @accent2@; }
QPushButton#play { border: none; border-radius: 20px; background: @accent@; }
QPushButton#play:hover { background: @accent2@; }
QPushButton#tool { padding: 7px 14px; }
QPushButton::menu-indicator { width: 0; image: none; }
QFrame#card { background: @panel@; border: none; border-radius: 12px; }
QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit, QPlainTextEdit { background: @bg@; border: 1px solid @border@;
    border-radius: 8px; padding: 6px 10px; }
QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus, QPlainTextEdit:focus { border-color: @muted@; }
QComboBox QAbstractItemView { background: @panel@; border: none; border-radius: 12px; padding: 6px;
    selection-background-color: @hover@; selection-color: @text@; }
QListWidget { background: transparent; border: none; padding: 0; }
QListWidget::item { border-radius: 8px; }
QListWidget::item:hover { background: @panel2@; }
QListWidget::item:selected { background: @hover@; color: @text@; }
QGraphicsView { border-radius: 12px; }
QScrollArea { border: none; border-radius: 12px; background: @tl@; }
QScrollBar::handle:horizontal, QScrollBar::handle:vertical { background: @hover@; }
QSlider::groove:horizontal { height: 4px; background: @border@; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
QSlider::handle:horizontal { width: 13px; height: 13px; margin: -5px 0; background: @accent@; border-radius: 6px; }
QToolTip { background: @panel2@; color: @text@; border: none; border-radius: 6px; padding: 6px 8px; }
QMenu { background: @panel@; border: 1px solid @border@; border-radius: 12px; padding: 8px 0; }
QMenu::item { padding: 8px 36px 8px 16px; color: @text@; }
QMenu::item:selected { background: @hover@; }
QCheckBox::indicator { width: 16px; height: 16px; border: 2px solid @muted@; border-radius: 3px; background: transparent; }
QCheckBox::indicator:checked { background: @accent@; border-color: @accent@; }
QProgressBar { border: none; background: @panel2@; border-radius: 3px; }
)";
    }
    if (t.xp) {
        // Windows XP "Luna": blaue Taskleiste, grüner Start-Knopf, blaues Aufgabenfenster, Luna-Scrollbalken
        s += R"(
* { font-family: Tahoma, Verdana, 'DejaVu Sans', sans-serif; font-size: 11px; }
QMainWindow, QDialog, QWidget#root { background: #ECE9D8; }

QWidget#topbar { border: 1px solid #0831D9; border-radius: 6px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3168D5, stop:0.06 #4993E6, stop:0.12 #2157D7,
                stop:0.5 #245EDB, stop:0.9 #1941A5, stop:1 #0F2E91); }
QWidget#topbar QPushButton { color: #FFFFFF; font-weight: bold; border: 1px solid #0F2E91; border-radius: 3px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5F9AF5, stop:0.1 #3E7BE8, stop:0.6 #2B63D6, stop:1 #1D4DB8); }
QWidget#topbar QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #7AB0FF, stop:0.6 #3D78EA, stop:1 #2658C9);
    border: 1px solid #0F2E91; }
QWidget#topbar QPushButton:pressed, QWidget#topbar QPushButton:checked {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1A45A6, stop:1 #2B63D6); border: 1px solid #0A246A; }
QWidget#topbar QPushButton#primary { font-style: italic; font-size: 13px; padding: 5px 20px 5px 16px;
    border: 1px solid #1F6B12; border-top-right-radius: 12px; border-bottom-right-radius: 12px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #7FD66A, stop:0.08 #4CB43A, stop:0.5 #3C9B3C,
                stop:0.9 #2F8A2A, stop:1 #226E1E); }
QWidget#topbar QPushButton#primary:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #9BE58A, stop:0.5 #4CB43A, stop:1 #2F8A2A); }

QPushButton { color: #000000; border: 1px solid #003C74; border-radius: 3px; padding: 4px 11px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #FFFFFF, stop:0.86 #ECEBE5, stop:1 #D6D0C5); }
QPushButton:hover { border: 1px solid #E5A01A; background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #FFF8E6, stop:0.1 #FFFFFF,
    stop:0.86 #ECEBE5, stop:0.94 #FAD68A, stop:1 #F8B636); }
QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #CDCAC3, stop:0.2 #E3E3DB, stop:1 #F2F1EC); }
QPushButton:checked { color: #000000; background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #E3E2DA, stop:1 #F9F8F3); border: 1px solid #003C74; }
QPushButton:disabled { color: #ACA899; border-color: #C9C7BA; }
QPushButton#play { border-radius: 20px; border: 1px solid #1B5E12; color: #FFFFFF;
    background: qradialgradient(cx:0.5, cy:0.28, radius:0.75, fx:0.5, fy:0.22, stop:0 #B9F59F, stop:0.35 #5CC43E,
                stop:0.8 #2E8B22, stop:1 #1F6B12); }
QPushButton#play:hover { background: qradialgradient(cx:0.5, cy:0.28, radius:0.75, fx:0.5, fy:0.22, stop:0 #D6FFC2,
                stop:0.35 #72D651, stop:0.8 #379D29, stop:1 #237A15); }
QPushButton#primary { color: #FFFFFF; font-weight: bold; border: 1px solid #1F6B12;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #7FD66A, stop:0.5 #3C9B3C, stop:1 #226E1E); }

QFrame#card { border: 1px solid #FFFFFF; border-radius: 6px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #7BA2E7, stop:1 #6375D6); }
QFrame#card QLabel { color: #FFFFFF; }
QFrame#card QLabel#title { color: #FFFFFF; font-weight: bold; font-size: 13px; }
QFrame#card QLabel#hint { color: #E3EBFF; }
QFrame#card QPushButton { color: #000000; }
/* Kopf eines XP-Aufgabenbereichs (wie links im Explorer) */
QFrame#card QPushButton#paneHeader { color: #215DC6; font-weight: bold; font-size: 12px; padding: 6px 10px; border: none;
    border-top-left-radius: 5px; border-top-right-radius: 5px; border-bottom-left-radius: 0; border-bottom-right-radius: 0;
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #FFFFFF, stop:1 #C6D3F7); }
QPlainTextEdit, QTextEdit { background: #FFFFFF; color: #000000; border: 1px solid #7F9DB9; border-radius: 0;
    selection-background-color: #316AC5; selection-color: #FFFFFF; }
QCheckBox { color: #000000; }
QFrame#card QCheckBox { color: #FFFFFF; }
QCheckBox::indicator { width: 13px; height: 13px; border: 1px solid #1C5180; background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #DCDCD7, stop:1 #FFFFFF); }
QCheckBox::indicator:checked { image: url(@xpcheck@); }

/* Abspielleiste als XP-Taskleiste mit grünem "Start"-Knopf */
QWidget#transport { border-top: 1px solid #3168D5;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3168D5, stop:0.06 #4993E6, stop:0.12 #2157D7,
                stop:0.5 #245EDB, stop:0.9 #1941A5, stop:1 #0F2E91); }
QWidget#transport QLabel { color: #FFFFFF; }
QWidget#transport QPushButton { color: #FFFFFF; font-weight: bold; border: 1px solid #0F2E91; border-radius: 3px; padding: 4px 11px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5F9AF5, stop:0.1 #3E7BE8, stop:0.6 #2B63D6, stop:1 #1D4DB8); }
QWidget#transport QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #7AB0FF, stop:0.6 #3D78EA, stop:1 #2658C9); }
QWidget#transport QPushButton:checked { color: #FFFFFF;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1A45A6, stop:1 #2B63D6); border: 1px solid #0A246A; }
QWidget#transport QPushButton#play { border-radius: 20px; border: 1px solid #1B5E12; padding: 0;
    background: qradialgradient(cx:0.5, cy:0.28, radius:0.75, fx:0.5, fy:0.22, stop:0 #B9F59F, stop:0.35 #5CC43E,
                stop:0.8 #2E8B22, stop:1 #1F6B12); }
QWidget#transport QPushButton#play:hover { background: qradialgradient(cx:0.5, cy:0.28, radius:0.75, fx:0.5, fy:0.22, stop:0 #D6FFC2,
                stop:0.35 #72D651, stop:0.8 #379D29, stop:1 #237A15); }
QWidget#transport QSlider::groove:horizontal { background: #1A3F9C; border: 1px solid #0A246A; border-bottom-color: #5F9AF5; }

QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit { background: #FFFFFF; color: #000000; border: 1px solid #7F9DB9;
    border-radius: 0; padding: 3px 5px; selection-background-color: #316AC5; selection-color: #FFFFFF; }
QComboBox::drop-down { width: 17px; border: 1px solid #C3D3FD; border-radius: 2px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #C6D3F7, stop:1 #9CB5EE); margin: 1px; }
QComboBox QAbstractItemView { background: #FFFFFF; color: #000000; border: 1px solid #7F9DB9;
    selection-background-color: #316AC5; selection-color: #FFFFFF; }
QListWidget { background: #FFFFFF; border: 1px solid #7F9DB9; border-radius: 0; }
QListWidget::item:selected { background: #316AC5; color: #FFFFFF; border-radius: 0; }

QScrollArea { border: 1px solid #7F9DB9; border-radius: 0; background: #FFFFFF; }
QScrollBar:horizontal { height: 17px; background: #F4F3EE; border-top: 1px solid #EEEDE5; }
QScrollBar:vertical { width: 17px; background: #F4F3EE; border-left: 1px solid #EEEDE5; }
QScrollBar::handle:horizontal, QScrollBar::handle:vertical { border: 1px solid #FFFFFF; border-radius: 3px; min-width: 24px; min-height: 24px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #CAD8FB, stop:0.5 #B8CCF8, stop:1 #A9C0F6); margin: 1px; }
QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #DDE7FF, stop:1 #BCD0FB); }

QSlider::groove:horizontal { height: 3px; background: #ECEBE6; border: 1px solid #9D9C99; border-bottom-color: #FFFFFF; border-radius: 0; }
QSlider::sub-page:horizontal { background: #316AC5; border: 1px solid #1C4A9E; border-radius: 0; }
QSlider::handle:horizontal { width: 11px; height: 21px; margin: -10px 0; border: 1px solid #1C5180; border-radius: 2px;
    border-bottom: 3px solid #3C9B3C;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #FFFFFF, stop:1 #E3E0D0); }
QSlider::handle:horizontal:hover { border-bottom: 3px solid #F8B636; }

QToolTip { background: #FFFFE1; color: #000000; border: 1px solid #000000; border-radius: 0; padding: 2px 4px; }
QProgressBar { background: #FFFFFF; border: 1px solid #686868; border-radius: 3px; padding: 1px; text-align: center; color: transparent; }
QProgressBar::chunk { width: 8px; margin: 1px; border-radius: 0;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ACEDAD, stop:0.3 #3EC23F, stop:0.7 #2FAF30, stop:1 #7AD47B); }
QMenu { background: #FFFFFF; border: 1px solid #ACA899; }
QMenu::item:selected { background: #316AC5; color: #FFFFFF; }
QGraphicsView { border: 1px solid #7F9DB9; border-radius: 0; }
QLabel#title { font-weight: bold; }
)";
    }
    if (t.xp) s.replace("@xpcheck@", xpCheckImage());
    auto hex = [](const QColor& c) { return c.name(); };
    s.replace("@font@", t.font).replace("@fs@", QString::number(t.fontPx)).replace("@fs2@", QString::number(t.fontPx + 3))
        .replace("@r@", QString::number(t.radius)).replace("@r6@", QString::number(std::max(0, t.radius - 3)))
        .replace("@bg@", hex(t.bg)).replace("@panel@", hex(t.panel)).replace("@panel2@", hex(t.panel2))
        .replace("@border@", hex(t.border)).replace("@text@", hex(t.text)).replace("@muted@", hex(t.muted))
        .replace("@accent2@", hex(t.accent.lighter(115))).replace("@accent@", hex(t.accent))
        .replace("@acctext@", hex(t.accentText)).replace("@tl@", hex(t.timeline)).replace("@video@", hex(t.video))
        .replace("@hover@", hex(t.hover));
    return s;
}
