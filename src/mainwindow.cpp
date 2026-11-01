#include "mainwindow.h"

#include <QAction>
#include <QButtonGroup>
#include <QColorDialog>
#include <QCursor>
#include <QFontDatabase>
#include <QScrollBar>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoSink>
#include <QMenu>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QAbstractSpinBox>
#include "dialogs.h"
#include "shortcuts.h"
#include "i18n.h"
#include "textrender.h"
#include "theme.h"

static constexpr double kMinPiece = 0.1;

// Startzeit messen (VIDLINE_TRACE=<Datei>): wann Fenster und erstes Bild da sind
QElapsedTimer g_startup;
static void trace(const QString& what) {
    const QString file = qEnvironmentVariable("VIDLINE_TRACE");
    if (file.isEmpty()) return;
    QFile f(file);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write(QString("%1 ms  %2\n").arg(g_startup.isValid() ? g_startup.elapsed() : -1).arg(what).toUtf8());
}
void vidlineTrace(const char* what) { trace(QString::fromUtf8(what)); }

enum class MediaKind { Video, Audio, Image };

static MediaKind mediaKindOf(const QString& path) {
    const QString e = QFileInfo(path).suffix().toLower();
    static const QStringList img = {"png", "jpg", "jpeg", "webp", "bmp", "gif"};
    static const QStringList aud = {"mp3", "wav", "m4a", "aac", "flac", "ogg", "opus", "wma", "aiff", "aif", "alac", "amr"};
    if (img.contains(e)) return MediaKind::Image;
    if (aud.contains(e)) return MediaKind::Audio;
    return MediaKind::Video;
}

// Tippt man gerade in ein Feld? Dann gehört die Leertaste dem Feld, nicht dem Player.
static bool typingFocus() {
    QWidget* f = QApplication::focusWidget();
    if (!f) return false;
    if (qobject_cast<QLineEdit*>(f) || qobject_cast<QPlainTextEdit*>(f) || qobject_cast<QTextEdit*>(f) ||
        qobject_cast<QAbstractSpinBox*>(f))
        return true;
    auto* cb = qobject_cast<QComboBox*>(f);
    return cb && cb->isEditable();
}

// Tempo-Stufen: unter 1× in 0,05er-Schritten, darüber immer größer
static const std::vector<double>& speedSteps() {
    static const std::vector<double> steps = [] {
        std::vector<double> r;
        auto add = [&r](double from, double to, double step) {
            for (int k = 0; from + k * step < to - 1e-9; ++k) r.push_back(std::round((from + k * step) * 100) / 100);
        };
        add(0.1, 1.0, 0.05);
        add(1.0, 2.0, 0.1);
        add(2.0, 4.0, 0.25);
        add(4.0, 8.0, 0.5);
        add(8.0, 16.0 + 1e-6, 1.0);
        return r;
    }();
    return steps;
}

static int nearestSpeedStep(double sp) {
    const auto& st = speedSteps();
    int best = 0;
    for (int i = 1; i < int(st.size()); ++i)
        if (std::abs(st[i] - sp) < std::abs(st[best] - sp)) best = i;
    return best;
}

MainWindow::MainWindow() {
    resize(1280, 820);
    setMinimumSize(900, 600);
    setAcceptDrops(true);
    ambientClock_.start();
    silent_ = qEnvironmentVariableIsSet("VIDLINE_MUTE");
    buildUi();
    applyTheme();
    retranslate();
    setEditMode(false);
    qApp->installEventFilter(this);  // Leertaste halten = 2× (braucht Drücken und Loslassen einzeln)

    // Updates: kurz nach dem Start still prüfen, fragt nur bei einer neuen (nicht ignorierten) Version
    Updater::Options uo;
    uo.repo = "erqf1/vidline";
    uo.appName = "Vidline";
    uo.version = APP_VERSION;
    uo.innoAppId = "B7D0C3E2-5A41-4C7E-9B61-3F2A8E6D1C90";  // packaging/windows/vidline.iss
    uo.parent = [this] { return static_cast<QWidget*>(this); };
    uo.texts = [] {
        return UpdaterTexts{T("upd_title"), T("upd_text"), T("upd_now"), T("upd_ignore"), T("upd_later"),
                            T("upd_downloading"), T("upd_failed"), T("upd_latest"), T("cancel")};
    };
    uo.beforeInstall = [this] {
        if (undo_.empty()) return true;
        return QMessageBox::question(this, "Vidline", T("upd_unsaved")) == QMessageBox::Yes;
    };
    updater_ = new Updater(uo, this);
    // Ton immer über das aktuelle Standardgerät ausgeben (Kopfhörer eingesteckt / Gerät gewechselt)
    auto* devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        const QAudioDevice d = QMediaDevices::defaultAudioOutput();
        audio_->setDevice(d);
        for (auto& [id, ap] : audioPlayers_) ap.out->setDevice(d);
    });
    if (!qEnvironmentVariableIsSet("VIDLINE_NO_UPDATE_CHECK"))
        updater_->startAutoCheck(4000);
}

// ---------------------------------------------------------------- Aufbau
// Medien-Panel: Einträge tragen beim Ziehen ihren Dateipfad (wie Dateien aus dem Explorer)
class BinList : public QListWidget {
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        auto* m = new QMimeData;
        QList<QUrl> urls;
        for (QListWidgetItem* it : items) urls << QUrl::fromLocalFile(it->data(Qt::UserRole).toString());
        m->setUrls(urls);
        return m;
    }
};

// Kurz aufleuchtendes Symbol über dem Video (wie bei YouTube): halbdurchsichtiger Kreis, weißes Zeichen,
// wächst leicht und blendet aus. Beim Spulen links bzw. rechts mit "5 s".
class Osd : public QWidget {
public:
    enum Kind { Play, Pause, Back, Forward };
    explicit Osd(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        anim_.setStartValue(0.0);
        anim_.setEndValue(1.0);
        anim_.setDuration(650);
        QObject::connect(&anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            t_ = v.toDouble();
            update();
        });
        QObject::connect(&anim_, &QVariantAnimation::finished, this, [this] { hide(); });
        hide();
    }
    void flash(Kind k) {
        kind_ = k;
        setGeometry(parentWidget()->rect());
        raise();
        show();
        anim_.stop();
        anim_.start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double fade = t_ < 0.35 ? 1.0 : 1.0 - (t_ - 0.35) / 0.65;  // erst stehen, dann ausblenden
        const double scale = 0.85 + 0.3 * t_;
        const double r = 42 * scale;
        QPointF c = rect().center();
        if (kind_ == Back) c.setX(width() * 0.2);
        if (kind_ == Forward) c.setX(width() * 0.8);
        p.setOpacity(std::clamp(fade, 0.0, 1.0));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 115));
        p.drawEllipse(c, r, r);
        p.setBrush(Qt::white);
        const double s = r * 0.42;
        if (kind_ == Play) {
            QPainterPath tri;
            tri.moveTo(c.x() - s * 0.7, c.y() - s);
            tri.lineTo(c.x() + s, c.y());
            tri.lineTo(c.x() - s * 0.7, c.y() + s);
            tri.closeSubpath();
            p.drawPath(tri);
        } else if (kind_ == Pause) {
            p.drawRoundedRect(QRectF(c.x() - s * 0.75, c.y() - s, s * 0.5, s * 2), 2, 2);
            p.drawRoundedRect(QRectF(c.x() + s * 0.25, c.y() - s, s * 0.5, s * 2), 2, 2);
        } else {
            // zwei kleine Pfeile + "5 s"
            const double dir = kind_ == Forward ? 1 : -1, a = s * 0.55, y = c.y() - s * 0.35;
            for (int i = 0; i < 2; ++i) {
                const double x = c.x() + dir * (i * a * 1.1 - a * 0.55);
                QPainterPath tri;
                tri.moveTo(x - dir * a * 0.5, y - a * 0.6);
                tri.lineTo(x + dir * a * 0.5, y);
                tri.lineTo(x - dir * a * 0.5, y + a * 0.6);
                tri.closeSubpath();
                p.drawPath(tri);
            }
            QFont f = font();
            f.setBold(true);
            f.setPixelSize(int(r * 0.32));
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(QRectF(c.x() - r, c.y() + s * 0.2, 2 * r, r * 0.5), Qt::AlignCenter, "5 s");
        }
    }

private:
    QVariantAnimation anim_;
    double t_ = 0;
    Kind kind_ = Play;
};

QPushButton* MainWindow::mk(const char* textKey, const char* tipKey, Ic ic, const char* objName,
                            const char* tipSuffix) {
    auto* b = new QPushButton;
    if (objName) b->setObjectName(objName);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    b->setIconSize(QSize(18, 18));
    const bool accent = objName && (QString(objName) == "primary" || QString(objName) == "play");
    btns_.push_back({b, textKey, tipKey, ic, tipSuffix, accent});
    return b;
}

QWidget* MainWindow::buildInspector() {
    insp_ = new QStackedWidget;

    // 0: Hinweis
    auto* hint = new QWidget;
    auto* hv = new QVBoxLayout(hint);
    lblInspHint_ = new QLabel;
    lblInspHint_->setObjectName("hint");
    lblInspHint_->setWordWrap(true);
    hv->addWidget(lblInspHint_);
    hv->addStretch();
    insp_->addWidget(hint);

    // 1: Abschnitt
    auto* pg = new QWidget;
    auto* v = new QVBoxLayout(pg);
    v->setSpacing(10);
    lblSegment_ = new QLabel;
    lblSegment_->setObjectName("title");
    v->addWidget(lblSegment_);
    lblSpeed_ = new QLabel;
    v->addWidget(lblSpeed_);
    // Tempo: Schieberegler (langsam feine, schnell größere Schritte) + Feld für jede beliebige Zahl
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    sliderSpeed_ = new QSlider(Qt::Horizontal);
    sliderSpeed_->setRange(0, int(speedSteps().size()) - 1);
    sliderSpeed_->setPageStep(1);
    sliderSpeed_->setFocusPolicy(Qt::NoFocus);
    connect(sliderSpeed_, &QSlider::valueChanged, this, [this](int i) {
        const double sp = speedSteps()[std::clamp<size_t>(i, 0, speedSteps().size() - 1)];
        QSignalBlocker b(spSpeed_);
        spSpeed_->setValue(sp);
        if (!sliderSpeed_->isSliderDown()) setSpeed(sp);  // Klick/Taste sofort, Ziehen erst beim Loslassen
    });
    connect(sliderSpeed_, &QSlider::sliderReleased, this, [this] { setSpeed(spSpeed_->value()); });
    row->addWidget(sliderSpeed_, 1);
    spSpeed_ = new QDoubleSpinBox;
    spSpeed_->setFixedWidth(78);
    spSpeed_->setRange(0.1, 16);
    spSpeed_->setSingleStep(0.25);
    spSpeed_->setDecimals(2);
    spSpeed_->setSuffix(" ×");
    spSpeed_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    connect(spSpeed_, &QDoubleSpinBox::editingFinished, this, [this] { setSpeed(spSpeed_->value()); });
    row->addWidget(spSpeed_);
    v->addLayout(row);
    lblPieceVol_ = new QLabel;
    v->addWidget(lblPieceVol_);
    sliderPieceVol_ = new QSlider(Qt::Horizontal);
    sliderPieceVol_->setRange(0, 600);  // bis 600 %
    sliderPieceVol_->setFocusPolicy(Qt::NoFocus);
    connect(sliderPieceVol_, &QSlider::sliderPressed, this, &MainWindow::pushUndo);
    connect(sliderPieceVol_, &QSlider::valueChanged, this, &MainWindow::setPieceVolume);
    v->addWidget(sliderPieceVol_);
    // Rauschunterdrückung (Tastatur/Maus/Rauschen) für den Ton dieses Abschnitts - wirkt beim Export
    chkPieceDenoise_ = new QCheckBox;
    chkPieceDenoise_->setFocusPolicy(Qt::NoFocus);
    connect(chkPieceDenoise_, &QCheckBox::toggled, this, [this](bool on) {
        if (selPiece_ < 0 || selPiece_ >= pr_.pieces.size()) return;
        pushUndo();
        pr_.pieces[selPiece_].denoise = on;
        modelEdited(true);
    });
    v->addWidget(chkPieceDenoise_);

    // Bild anpassen: Größe, Position, Drehung (wie in gängigen Schnittprogrammen)
    auto spin = [](double lo, double hi, int dec, const QString& suffix) {
        auto* sp = new QDoubleSpinBox;
        sp->setRange(lo, hi);
        sp->setDecimals(dec);
        sp->setSuffix(suffix);
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sp->setFixedWidth(78);
        return sp;
    };
    lblSize_ = new QLabel;
    v->addWidget(lblSize_);
    auto* srow = new QHBoxLayout;
    sliderScale_ = new QSlider(Qt::Horizontal);
    sliderScale_->setRange(10, 400);
    sliderScale_->setFocusPolicy(Qt::NoFocus);
    spScale_ = spin(10, 400, 0, " %");
    connect(sliderScale_, &QSlider::sliderPressed, this, &MainWindow::pushUndo);
    connect(sliderScale_, &QSlider::valueChanged, this, [this](int val) {
        if (!sliderScale_->isSliderDown()) pushUndo();
        QSignalBlocker b(spScale_);
        spScale_->setValue(val);
        editPieceTf([val](Piece& p) { p.scale = val / 100.0; });
    });
    connect(spScale_, &QDoubleSpinBox::editingFinished, this, [this] {
        pushUndo();
        const double val = spScale_->value();
        { QSignalBlocker b(sliderScale_); sliderScale_->setValue(qRound(val)); }
        editPieceTf([val](Piece& p) { p.scale = val / 100.0; });
    });
    srow->addWidget(sliderScale_, 1);
    srow->addWidget(spScale_);
    v->addLayout(srow);
    lblPos_ = new QLabel;
    v->addWidget(lblPos_);
    auto* prow = new QHBoxLayout;
    spPosX_ = spin(-200, 200, 1, " %");
    spPosY_ = spin(-200, 200, 1, " %");
    for (auto [sp, axis] : {std::pair{spPosX_, 'x'}, std::pair{spPosY_, 'y'}}) {
        prow->addWidget(new QLabel(axis == 'x' ? "X" : "Y"));
        prow->addWidget(sp);
        connect(sp, &QDoubleSpinBox::editingFinished, this, [this, sp = sp, axis = axis] {
            pushUndo();
            const double val = sp->value() / 100.0;
            editPieceTf([val, axis](Piece& p) { (axis == 'x' ? p.px : p.py) = val; });
        });
    }
    prow->addStretch();
    v->addLayout(prow);
    lblRot_ = new QLabel;
    v->addWidget(lblRot_);
    auto* rrow = new QHBoxLayout;
    spRot_ = spin(-360, 360, 1, " °");
    connect(spRot_, &QDoubleSpinBox::editingFinished, this, [this] {
        pushUndo();
        const double val = spRot_->value();
        editPieceTf([val](Piece& p) { p.rot = val; });
    });
    rrow->addWidget(spRot_);
    rrow->addStretch();
    btnResetTf_ = new QPushButton;
    btnResetTf_->setFocusPolicy(Qt::NoFocus);
    connect(btnResetTf_, &QPushButton::clicked, this, [this] {
        pushUndo();
        editPieceTf([](Piece& p) { p.scale = 1; p.px = p.py = p.rot = 0; });
        refreshInspector();
    });
    rrow->addWidget(btnResetTf_);
    v->addLayout(rrow);
    lblPiece_ = new QLabel;
    lblPiece_->setObjectName("hint");
    lblPiece_->setWordWrap(true);
    v->addWidget(lblPiece_);
    v->addStretch();
    insp_->addWidget(pg);

    // 2: Element (Bild / Unschärfe / Ton)
    pg = new QWidget;
    v = new QVBoxLayout(pg);
    v->setSpacing(10);
    lblItem_ = new QLabel;
    lblItem_->setObjectName("title");
    v->addWidget(lblItem_);
    spT0_ = new QDoubleSpinBox;
    spT1_ = new QDoubleSpinBox;
    lblStart_ = new QLabel;
    lblEnd_ = new QLabel;
    int n = 0;
    for (QDoubleSpinBox* sp : {spT0_, spT1_}) {
        sp->setRange(0, 36000);
        sp->setDecimals(2);
        sp->setSingleStep(0.25);
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        connect(sp, &QDoubleSpinBox::editingFinished, this, &MainWindow::itemTimesChanged);
        v->addWidget(n++ == 0 ? lblStart_ : lblEnd_);
        v->addWidget(sp);
    }
    lblStr_ = new QLabel;
    v->addWidget(lblStr_);
    sliderStr_ = new QSlider(Qt::Horizontal);
    sliderStr_->setRange(5, 100);
    sliderStr_->setFocusPolicy(Qt::NoFocus);
    connect(sliderStr_, &QSlider::sliderPressed, this, &MainWindow::pushUndo);
    connect(sliderStr_, &QSlider::valueChanged, this, &MainWindow::sliderChanged);
    v->addWidget(sliderStr_);
    chkAudioDenoise_ = new QCheckBox;
    chkAudioDenoise_->setFocusPolicy(Qt::NoFocus);
    connect(chkAudioDenoise_, &QCheckBox::toggled, this, [this](bool on) {
        AudioClip* a = selAudio_ ? pr_.audio(selAudio_) : nullptr;
        if (!a) return;
        pushUndo();
        a->denoise = on;
        modelEdited(true);
    });
    v->addWidget(chkAudioDenoise_);

    textBox_ = new QWidget;
    auto* tv = new QVBoxLayout(textBox_);
    tv->setContentsMargins(0, 0, 0, 0);
    tv->setSpacing(8);
    textEdit_ = new QPlainTextEdit;
    textEdit_->setFixedHeight(64);
    connect(textEdit_, &QPlainTextEdit::textChanged, this, [this] {
        Item* it = pr_.item(selItem_);
        if (!it || it->kind != Item::Text || it->text == textEdit_->toPlainText()) return;
        if (!textUndo_) { pushUndo(); textUndo_ = true; }
        it->text = textEdit_->toPlainText();
        for (Overlay* ov : overlays_) ov->update();
        timeline_->update();
    });
    tv->addWidget(textEdit_);
    lblFont_ = new QLabel;
    tv->addWidget(lblFont_);
    fontBox_ = new QComboBox;  // Inhalt erst beim ersten Text (ensureFonts) - alle Schriften aufzählen dauert
    fontBox_->setMaxVisibleItems(16);
    connect(fontBox_, &QComboBox::activated, this, [this] {
        Item* it = pr_.item(selItem_);
        if (!it || it->kind != Item::Text) return;
        pushUndo();
        it->font = fontBox_->currentData().toString();
        for (Overlay* ov : overlays_) ov->update();
    });
    tv->addWidget(fontBox_);
    lblColors_ = new QLabel;
    tv->addWidget(lblColors_);
    auto* crow = new QHBoxLayout;
    btnColor_ = new QPushButton;
    btnBgColor_ = new QPushButton;
    chkBg_ = new QCheckBox;
    for (QPushButton* b : {btnColor_, btnBgColor_}) { b->setFixedSize(34, 26); b->setFocusPolicy(Qt::NoFocus); }
    connect(btnColor_, &QPushButton::clicked, this, [this] { pickItemColor(false); });
    connect(btnBgColor_, &QPushButton::clicked, this, [this] { pickItemColor(true); });
    connect(chkBg_, &QCheckBox::toggled, this, [this](bool on) {
        Item* it = pr_.item(selItem_);
        if (!it || it->kind != Item::Text || it->bg == on) return;
        pushUndo();
        it->bg = on;
        btnBgColor_->setEnabled(on);
        for (Overlay* ov : overlays_) ov->update();
    });
    crow->addWidget(btnColor_);
    crow->addSpacing(10);
    crow->addWidget(chkBg_);
    crow->addWidget(btnBgColor_);
    crow->addStretch();
    tv->addLayout(crow);
    v->addWidget(textBox_);

    // Zeichnung / Form: Farbe und (bei Rechteck und Kreis) Füllen
    shapeBox_ = new QWidget;
    auto* sb = new QHBoxLayout(shapeBox_);
    sb->setContentsMargins(0, 0, 0, 0);
    lblShapeColor_ = new QLabel;
    btnShapeColor_ = new QPushButton;
    btnShapeColor_->setFixedSize(34, 26);
    btnShapeColor_->setFocusPolicy(Qt::NoFocus);
    connect(btnShapeColor_, &QPushButton::clicked, this, [this] { pickItemColor(false); });
    chkFill_ = new QCheckBox;
    chkFill_->setFocusPolicy(Qt::NoFocus);
    connect(chkFill_, &QCheckBox::toggled, this, [this](bool on) {
        Item* it = pr_.item(selItem_);
        if (!it || it->kind != Item::Shape || it->fill == on) return;
        pushUndo();
        it->fill = on;
        for (Overlay* ov : overlays_) ov->update();
    });
    sb->addWidget(lblShapeColor_);
    sb->addWidget(btnShapeColor_);
    sb->addSpacing(10);
    sb->addWidget(chkFill_);
    sb->addStretch();
    v->addWidget(shapeBox_);
    btnDelEl_ = mk("delete_el", nullptr, Ic::Trash);
    connect(btnDelEl_, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    v->addWidget(btnDelEl_);
    v->addStretch();
    insp_->addWidget(pg);

    // Medien links vom Video, Eigenschaften rechts davon - beide immer sichtbar
    auto card = [](int width) {
        auto* c = new QFrame;
        c->setObjectName("card");
        c->setMinimumWidth(width);  // Breite per Trenner verstellbar (wie bei CapCut)
        auto* l = new QVBoxLayout(c);
        l->setContentsMargins(12, 12, 12, 12);
        l->setSpacing(10);
        return c;
    };
    // Überschrift mit Symbol (flach, nicht anklickbar - sieht nicht wie ein Knopf aus)
    auto header = [this](const char* text, Ic ic) {
        QPushButton* b = mk(text, nullptr, ic);
        b->setFlat(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setAttribute(Qt::WA_TransparentForMouseEvents);
        b->setObjectName("paneHeader");  // Aussehen im Theme (bei XP: Kopf eines Aufgabenbereichs)
        return b;
    };
    mediaCard_ = card(190);
    tabMedia_ = header("media", Ic::AddVideo);
    mediaCard_->layout()->addWidget(tabMedia_);
    mediaCard_->layout()->addWidget(buildMediaPage());

    inspCard_ = card(230);
    auto* cl = static_cast<QVBoxLayout*>(inspCard_->layout());
    tabProps_ = header("properties", Ic::Edit);
    cl->addWidget(tabProps_);
    // Eigenschaften scrollen, wenn sie nicht ganz hineinpassen
    auto* inspScroll = new QScrollArea;
    inspScroll->setObjectName("plain");
    inspScroll->setWidget(insp_);
    inspScroll->setWidgetResizable(true);
    inspScroll->setFrameShape(QFrame::NoFrame);
    inspScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inspScroll->setStyleSheet("QScrollArea#plain { border: none; background: transparent; }"
                              "QScrollArea#plain > QWidget > QWidget { background: transparent; }");
    cl->addWidget(inspScroll, 1);
    return inspCard_;
}

void MainWindow::buildUi() {
    // Player + Videoansicht
    player_ = new QMediaPlayer(this);
    audio_ = new QAudioOutput(this);
    audio_->setVolume(float(masterVol_));
    audio_->setMuted(silent_);
    player_->setAudioOutput(audio_);
    scene_ = new QGraphicsScene(this);
    canvasBg_ = scene_->addRect(0, 0, 1920, 1080, Qt::NoPen, Qt::NoBrush);  // schwarz erst mit geladenem Video
    canvasBg_->setZValue(-1);
    canvasBg_->setFlag(QGraphicsItem::ItemClipsChildrenToShape);  // verschobenes/vergrößertes Video bleibt im Bild
    vitem_ = new QGraphicsVideoItem(canvasBg_);
    player_->setVideoOutput(vitem_);
    view_ = new VideoView(scene_);
    connect(vitem_, &QGraphicsVideoItem::nativeSizeChanged, this, &MainWindow::onNativeSize);
    connect(vitem_->videoSink(), &QVideoSink::videoFrameChanged, this, &MainWindow::onFrame);
    connect(player_, &QMediaPlayer::durationChanged, this, &MainWindow::onDuration);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onState);
    connect(player_, &QMediaPlayer::mediaStatusChanged, this, &MainWindow::onStatus);
    connect(view_, &VideoView::emptyClicked, this, [this] { pr_.pieces.isEmpty() ? openDialog() : togglePlay(); });
    connect(view_, &VideoView::toolPressed, this, &MainWindow::toolPress);
    connect(view_, &VideoView::toolMoved, this, &MainWindow::toolMove);
    connect(view_, &VideoView::toolReleased, this, &MainWindow::toolRelease);
    timer_ = new QTimer(this);
    timer_->setInterval(15);
    connect(timer_, &QTimer::timeout, this, &MainWindow::tick);
    // Leiste über dem Bild: verschwindet nach ein paar Sekunden ohne Mausbewegung (auch pausiert),
    // außer die Maus liegt gerade auf ihr. Beim Abspielen (und im Vollbild) geht auch der Mauszeiger weg.
    barTimer_ = new QTimer(this);
    barTimer_->setSingleShot(true);
    barTimer_->setInterval(2600);
    connect(barTimer_, &QTimer::timeout, this, [this] {
        if (!overlayMode() || pr_.pieces.isEmpty()) return;
        const bool onBar = fsBar_->isVisible() && fsBar_->rect().contains(fsBar_->mapFromGlobal(QCursor::pos()));
        if (onBar || fsSlider_->isSliderDown() || scrubbing_) {
            barTimer_->start();
            return;
        }
        barShown_ = false;
        const QPoint m = view_->viewport()->mapFromGlobal(QCursor::pos());
        if (view_->viewport()->rect().contains(m) && (fullscreen_ || playingIntent()))
            view_->viewport()->setCursor(Qt::BlankCursor);
        placeVideoControls();
    });
    connect(view_, &VideoView::mouseActivity, this, [this] { showBar(); });
    // Leertaste: kurz = Pause/Abspielen, gedrückt halten = 2× Tempo bis zum Loslassen
    holdTimer_ = new QTimer(this);
    holdTimer_->setSingleShot(true);
    holdTimer_->setInterval(320);
    connect(holdTimer_, &QTimer::timeout, this, [this] {
        if (playKeyDown_ && playingIntent()) setBoost(true);
    });
    unmuteTimer_ = new QTimer(this);
    unmuteTimer_->setSingleShot(true);
    unmuteTimer_->setInterval(220);
    connect(unmuteTimer_, &QTimer::timeout, this, [this] {
        if (!scrubbing_ && !silent_) audio_->setMuted(false);
    });

    // Obere Leiste
    topbar_ = new QWidget;
    topbar_->setObjectName("topbar");
    auto* top = new QHBoxLayout(topbar_);
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(8);
    auto* bOpen = mk("open", "open_video", Ic::Open, nullptr, "@open");
    auto* bExp = mk("export", nullptr, Ic::Export, "primary", "@export");
    connect(bOpen, &QPushButton::clicked, this, &MainWindow::openDialog);
    connect(bExp, &QPushButton::clicked, this, &MainWindow::exportVideo);
    top->addWidget(bOpen);
    top->addStretch();
    top->addWidget(bExp);

    // Transportleiste
    transport_ = new QWidget;
    transport_->setObjectName("transport");
    transport_->setAttribute(Qt::WA_StyledBackground);  // damit Themes (XP: Taskleiste) einen Hintergrund setzen können
    auto* bar = new QHBoxLayout(transport_);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(12);
    btnPlay_ = mk(nullptr, "play_tip", Ic::Play, "play", "@play");
    connect(btnPlay_, &QPushButton::clicked, this, &MainWindow::togglePlay);
    seekTimer_ = new QTimer(this);
    seekTimer_->setSingleShot(true);
    connect(seekTimer_, &QTimer::timeout, this, &MainWindow::pumpScrub);
    lblTime_ = new QLabel("00:00.0 / 00:00.0");
    btnEdit_ = mk("edit", nullptr, Ic::Edit, nullptr, "@edit");
    btnEdit_->setCheckable(true);
    connect(btnEdit_, &QPushButton::toggled, this, &MainWindow::setEditMode);
    auto* bSet = mk(nullptr, "settings", Ic::Settings);
    connect(bSet, &QPushButton::clicked, this, &MainWindow::openSettings);
    slider_ = new SeekSlider;
    slider_->setRange(0, 10000);
    slider_->setFocusPolicy(Qt::NoFocus);
    wireSeekSlider(slider_);
    bar->addWidget(btnPlay_);
    bar->addWidget(slider_, 1);
    seekGap_ = new QWidget;  // beim Bearbeiten statt der Leiste: dort spult man über die Zeitleiste
    seekGap_->hide();
    bar->addWidget(seekGap_, 1);
    bar->addWidget(lblTime_);
    bar->addWidget(btnEdit_);
    bar->addWidget(bSet);

    // Werkzeuge
    tools_ = new QWidget;
    auto* tl = new QHBoxLayout(tools_);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(8);
    struct ToolDef { const char* text; const char* tip; Ic ic; const char* suffix; void (MainWindow::*fn)(); };
    const ToolDef list[] = {
        {"split", nullptr, Ic::Scissors, "@split", &MainWindow::doSplit},
        {"remove_piece", nullptr, Ic::Trash, "@delete", &MainWindow::doDelete},
        {"blur", nullptr, Ic::Blur, "@blur", &MainWindow::addBlur},
        {"text", nullptr, Ic::Text, "@text", &MainWindow::addText},
    };
    for (const ToolDef& t : list) {
        auto* b = mk(t.text, t.tip, t.ic, "tool", t.suffix);
        connect(b, &QPushButton::clicked, this, [this, fn = t.fn] { (this->*fn)(); });
        tl->addWidget(b);
    }
    // Zeichnen (an/aus) und Formen (Menü: Rechteck, Kreis, Pfeil, Linie)
    btnDraw_ = mk("draw", nullptr, Ic::Pen, "tool", "@draw");
    btnDraw_->setCheckable(true);
    connect(btnDraw_, &QPushButton::clicked, this, [this] { setTool(tool_ == 1 ? 0 : 1); });
    tl->addWidget(btnDraw_);
    btnShape_ = mk("shape", nullptr, Ic::Shapes, "tool", "@shape");
    btnShape_->setCheckable(true);
    auto* shapeMenu = new QMenu(btnShape_);
    connect(shapeMenu, &QMenu::aboutToShow, this, [this, shapeMenu] {
        shapeMenu->clear();
        const char* keys[] = {"shape_rect", "shape_ellipse", "shape_arrow", "shape_line"};
        for (int k = 0; k < 4; ++k)
            shapeMenu->addAction(T(keys[k]), this, [this, k] { shapeKind_ = k; setTool(2); });
    });
    connect(shapeMenu, &QMenu::aboutToHide, this, [this] { btnShape_->setChecked(tool_ == 2); });
    btnShape_->setMenu(shapeMenu);
    tl->addWidget(btnShape_);
    for (const ToolDef& t : {ToolDef{nullptr, "undo", Ic::Undo, "@undo", &MainWindow::doUndo},
                       ToolDef{nullptr, "redo", Ic::Redo, "@redo", &MainWindow::doRedo}}) {
        auto* b = mk(t.text, t.tip, t.ic, "tool", t.suffix);
        connect(b, &QPushButton::clicked, this, [this, fn = t.fn] { (this->*fn)(); });
        tl->addWidget(b);
    }
    tl->addSpacing(8);
    lblAspect_ = new QLabel;
    lblAspect_->setObjectName("hint");
    tl->addWidget(lblAspect_);
    aspectBox_ = new QComboBox;
    for (int i = 0; i < kAspectCount; ++i) aspectBox_->addItem(kAspects[i].label ? QString::fromUtf8(kAspects[i].label) : QString());
    aspectBox_->setFocusPolicy(Qt::NoFocus);  // Tastenkürzel (S, Q, W …) bleiben beim Editor
    connect(aspectBox_, &QComboBox::activated, this, [this](int i) { setAspect(i); });
    tl->addWidget(aspectBox_);
    tl->addStretch();
    auto* zOut = mk(nullptr, "zoom_out", Ic::ZoomOut, "tool", "@zoom_out");
    auto* zIn = mk(nullptr, "zoom_in", Ic::ZoomIn, "tool", "@zoom_in");
    connect(zOut, &QPushButton::clicked, this, [this] { timeline_->zoomBy(1 / 1.6); });
    connect(zIn, &QPushButton::clicked, this, [this] { timeline_->zoomBy(1.6); });
    tl->addWidget(zOut);
    tl->addWidget(zIn);

    // Timeline
    timeline_ = new Timeline(this);
    tlScroll_ = new QScrollArea;
    tlScroll_->setWidget(timeline_);
    tlScroll_->setWidgetResizable(false);
    tlScroll_->setMinimumHeight(130);
    tlScroll_->viewport()->installEventFilter(this);

    // Bereiche mit Trennern: Medien | Video | Eigenschaften, darunter Abspielen + Werkzeuge + Zeitleiste.
    // Alles per Ziehen größer und kleiner (Größen werden gemerkt).
    QWidget* inspector = buildInspector();
    hSplit_ = new QSplitter(Qt::Horizontal);
    hSplit_->setChildrenCollapsible(false);
    hSplit_->setHandleWidth(10);
    hSplit_->addWidget(mediaCard_);
    hSplit_->addWidget(view_);
    hSplit_->addWidget(inspector);
    hSplit_->setStretchFactor(0, 0);
    hSplit_->setStretchFactor(1, 1);
    hSplit_->setStretchFactor(2, 0);
    hSplit_->setSizes({250, 760, 290});
    editBottom_ = new QWidget;
    auto* eb = new QVBoxLayout(editBottom_);
    eb->setContentsMargins(0, 0, 0, 0);
    eb->setSpacing(10);
    eb->addWidget(transport_);
    eb->addWidget(tools_);
    eb->addWidget(tlScroll_, 1);
    vSplit_ = new QSplitter(Qt::Vertical);
    vSplit_->setChildrenCollapsible(false);
    vSplit_->setHandleWidth(10);
    vSplit_->addWidget(hSplit_);
    vSplit_->addWidget(editBottom_);
    vSplit_->setStretchFactor(0, 1);
    vSplit_->setStretchFactor(1, 0);
    vSplit_->setSizes({520, 300});
    {
        QSettings st;
        hSplit_->restoreState(st.value("layout/h").toByteArray());
        vSplit_->restoreState(st.value("layout/v").toByteArray());
    }
    for (QSplitter* sp : {hSplit_, vSplit_})
        connect(sp, &QSplitter::splitterMoved, this, [this] {
            QSettings st;
            st.setValue("layout/h", hSplit_->saveState());
            st.setValue("layout/v", vSplit_->saveState());
            fitView();
        });
    auto* root = new QWidget;
    root->setObjectName("root");
    rootLayout_ = new QVBoxLayout(root);
    rootLayout_->setContentsMargins(14, 12, 14, 14);
    rootLayout_->setSpacing(10);
    rootLayout_->addWidget(topbar_);
    rootLayout_->addWidget(vSplit_, 1);
    setCentralWidget(root);

    // Vollbild-Knopf direkt im Video (unten rechts)
    fsBtn_ = new QPushButton(view_);
    fsBtn_->setObjectName("vidbtn");
    fsBtn_->setFocusPolicy(Qt::NoFocus);
    fsBtn_->setCursor(Qt::PointingHandCursor);
    fsBtn_->setFixedSize(40, 40);
    fsBtn_->setIconSize(QSize(18, 18));
    fsBtn_->setStyleSheet("QPushButton#vidbtn { background: rgba(0,0,0,0.45); border: none; border-radius: 20px; }"
                          "QPushButton#vidbtn:hover { background: rgba(0,0,0,0.7); }");
    connect(fsBtn_, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);

    // Vollbild: schmale Leiste über dem Bild - Pause, Fortschritt, Zeit. Verschwindet nach ein paar Sekunden.
    const QString vidBtn = "QPushButton { background: transparent; border: none; border-radius: 7px; color: #ffffff; padding: 0 6px; }"
                           "QPushButton:hover { background: rgba(255,255,255,0.14); }";
    fsBar_ = new QFrame(view_);
    fsBar_->setObjectName("fsbar");
    fsBar_->setStyleSheet("QFrame#fsbar { background: rgba(12,13,18,0.72); border: 1px solid rgba(255,255,255,0.14); border-radius: 10px; }"
                          "QLabel { color: #ffffff; background: transparent; font-size: 12px; }"
                          "QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,0.25); border-radius: 2px; }"
                          "QSlider::sub-page:horizontal { background: #ffffff; border-radius: 2px; }"
                          "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; background: #ffffff; }");
    fsBar_->setFixedHeight(36);
    auto* fb = new QHBoxLayout(fsBar_);
    fb->setContentsMargins(6, 2, 6, 2);
    fb->setSpacing(8);
    fsPlay_ = new QPushButton;
    fsPlay_->setFixedSize(28, 28);
    fsPlay_->setIconSize(QSize(16, 16));
    fsPlay_->setFocusPolicy(Qt::NoFocus);
    fsPlay_->setCursor(Qt::PointingHandCursor);
    fsPlay_->setStyleSheet(vidBtn);
    fsSlider_ = new SeekSlider;
    fsSlider_->setRange(0, 10000);
    fsSlider_->setFocusPolicy(Qt::NoFocus);
    wireSeekSlider(fsSlider_);
    fsTime_ = new QLabel;
    fb->addWidget(fsPlay_);
    fb->addWidget(fsSlider_, 1);
    fb->addWidget(fsTime_);
    connect(fsPlay_, &QPushButton::clicked, this, &MainWindow::togglePlay);
    fsBar_->hide();
    fsHint_ = new QLabel(view_);
    fsHint_->setAlignment(Qt::AlignCenter);
    fsHint_->setStyleSheet("color:#ffffff;background:rgba(0,0,0,0.65);border-radius:12px;padding:10px 18px;font-size:15px;font-weight:600;");
    fsHint_->hide();
    // "2×" oben im Bild, solange die Leertaste gehalten wird
    boostLbl_ = new QLabel(view_);
    boostLbl_->setAttribute(Qt::WA_TransparentForMouseEvents);
    boostLbl_->setStyleSheet("color:#ffffff;background:rgba(0,0,0,0.6);border-radius:16px;padding:6px 16px;font-size:14px;font-weight:700;");
    boostLbl_->hide();
    view_->installEventFilter(this);

    scrubPrev_ = new QLabel(this);
    scrubPrev_->setAttribute(Qt::WA_TransparentForMouseEvents);
    scrubPrev_->hide();

    lblHint_ = new QLabel(view_);
    lblHint_->setStyleSheet("color:#8b90a0;font-size:17px;background:transparent");

    // Alle Tastenkürzel über Kennungen - welche Taste, steht in den Einstellungen (Einstellungen -> Tastenkürzel)
    addShortcut("play", [this] { togglePlay(); });
    addShortcut("open", [this] { openDialog(); });
    addShortcut("export", [this] { exportVideo(); });
    addShortcut("edit", [this] { btnEdit_->toggle(); });
    addShortcut("fullscreen", [this] { toggleFullscreen(); });
    addShortcut("exitfs", [this] {
        if (tool_) setTool(0);
        else if (fullscreen_) exitFullscreen();
    });
    addShortcut("split", [this] { doSplit(); });
    addShortcut("delete", [this] { doDelete(); });
    addShortcut("trim_start", [this] { doTrimStart(); });
    addShortcut("trim_end", [this] { doTrimEnd(); });
    addShortcut("blur", [this] { addBlur(); });
    addShortcut("image", [this] { addImage(); });
    addShortcut("text", [this] { addText(); });
    addShortcut("draw", [this] { setTool(tool_ == 1 ? 0 : 1); });
    addShortcut("shape", [this] { setTool(tool_ == 2 ? 0 : 2); });
    addShortcut("zoom_in", [this] { timeline_->zoomBy(1.6); });
    addShortcut("zoom_out", [this] { timeline_->zoomBy(1 / 1.6); });
    addShortcut("undo", [this] { doUndo(); });
    addShortcut("redo", [this] { doRedo(); });
    addShortcut("back", [this] {
        seek(t_ - 5.0);
        if (osd_ && !pr_.pieces.isEmpty()) osd_->flash(Osd::Back);
        showBar();  // wie bei YouTube: beim Spulen kurz zeigen, wo man ist
    });
    addShortcut("fwd", [this] {
        seek(t_ + 5.0);
        if (osd_ && !pr_.pieces.isEmpty()) osd_->flash(Osd::Forward);
        showBar();
    });
    osd_ = new Osd(view_);
    applyShortcuts();
}

void MainWindow::addShortcut(const QString& id, std::function<void()> fn) {
    auto* a = new QAction(this);
    connect(a, &QAction::triggered, this, [fn] { fn(); });
    addAction(a);
    scActions_[id] = a;
}

void MainWindow::applyShortcuts() {
    // Abspiel-Taste ohne Umschalt/Strg (Standard: Leertaste) behandeln wir selbst: kurz = Pause, halten = 2×
    const QKeySequence pk = shortcutFor("play");
    playKey_ = 0;
    if (pk.count() == 1 && pk[0].keyboardModifiers() == Qt::NoModifier) playKey_ = pk[0].key();
    for (auto& [id, a] : scActions_) {
        QList<QKeySequence> keys;
        const QKeySequence k = shortcutFor(id);
        if (!k.isEmpty() && !(id == "play" && playKey_)) keys << k;
        // Gewohnte Zweitbelegungen, solange die Standardtaste aktiv ist
        if (k == defaultShortcut(id)) {
            if (id == "zoom_in") keys << QKeySequence("Ctrl+=");
            if (id == "redo") keys << QKeySequence("Ctrl+Shift+Z");
        }
        a->setShortcuts(keys);
    }
    retranslate();  // Tooltips zeigen die aktuelle Taste
}

QIcon MainWindow::themedIcon(Ic ic, const QColor& normal, const QColor& checked) const {
    QIcon i = makeIcon(ic, normal);
    i.addPixmap(makeIcon(ic, checked).pixmap(96, 96), QIcon::Normal, QIcon::On);
    return i;
}

void MainWindow::applyTheme() {
    const Theme& th = currentTheme();
    qApp->setStyleSheet(buildStyleSheet(th));
    // ausgewählte Knöpfe: YouTube-Stil = helle Fläche (Symbol in Hintergrundfarbe), sonst Akzentfläche
    const QColor onColor = th.yt ? th.bg : th.accentText;
    for (const BtnSpec& s : btns_) {
        if (s.b == btnPlay_) continue;
        s.b->setProperty("dummy", 0);
        const bool onBar = th.xp && topbar_->isAncestorOf(s.b);  // XP-Taskleiste: weiße Symbole
        const QColor c = s.onAccent || onBar ? th.accentText : th.text;
        static_cast<QPushButton*>(s.b)->setIcon(s.b->isCheckable() ? themedIcon(s.icon, c, onColor) : makeIcon(s.icon, c));
    }
    lblHint_->setStyleSheet(th.xp ? QString("color:#FFFFFF;font-size:17px;font-weight:bold;background:transparent")
                                  : QString("color:%1;font-size:17px;background:transparent").arg(th.muted.name()));
    // XP: Taskleiste mit Innenabstand, sonst bündig
    if (auto* l = topbar_->layout()) l->setContentsMargins(th.xp ? QMargins(6, 4, 6, 4) : QMargins(0, 0, 0, 0));
    updatePlayIcon();
    placeVideoControls();
    view_->viewport()->update();
    timeline_->update();
    for (Overlay* ov : overlays_) ov->update();
}

void MainWindow::updatePlayIcon() {
    const Theme& th = currentTheme();
    // beim Spulen zählt, ob es danach weiterläuft (das kurze Anspielen soll nicht flackern)
    const bool playing = scrubbing_ ? scrubResume_ : player_->playbackState() == QMediaPlayer::PlayingState;
    btnPlay_->setIcon(makeIcon(playing ? Ic::Pause : Ic::Play, th.accentText));
    btnPlay_->setIconSize(QSize(20, 20));
    if (fsPlay_) fsPlay_->setIcon(makeIcon(playing ? Ic::Pause : Ic::Play, Qt::white));
}

bool MainWindow::playingIntent() const {
    // Beim Wechsel auf ein anderes Video pausiert der Player kurz - das zählt nicht als Pause
    return player_->playbackState() == QMediaPlayer::PlayingState || (pending_ && pendingPlay_) ||
           (scrubbing_ && scrubResume_);
}

// Nur im Vollbild liegt die Leiste über dem Bild und blendet sich aus
bool MainWindow::overlayMode() const { return fullscreen_; }

bool MainWindow::barVisible() const { return overlayMode() && (pr_.pieces.isEmpty() || barShown_); }

// Leiste (wieder) zeigen - nach ein paar Sekunden ohne Mausbewegung verschwindet sie von selbst
void MainWindow::showBar() {
    if (!overlayMode()) return;
    if (!tool_) view_->viewport()->unsetCursor();
    barTimer_->start();
    if (!barShown_) {
        barShown_ = true;
        placeVideoControls();
    }
}

// Leiste: nur im Vollbild, schmal und mittig über dem unteren Bildrand
void MainWindow::updateFsBar() {
    if (!fsBar_) return;
    const bool show = barVisible();
    const int w = std::min(view_->width() - 28, 900);
    fsBar_->setGeometry((view_->width() - w) / 2, view_->height() - fsBar_->height() - 14, w, fsBar_->height());
    fsBar_->setVisible(show);
    if (show) fsBar_->raise();
    fsBtn_->setVisible(!fullscreen_ || show);
    fsPlay_->setToolTip(T("sc_play") + " (" + shortcutText("play") + ")");
    updatePlayIcon();
    updateUi();
}

void MainWindow::retranslate() {
    // Qt haengt unter Windows/Linux selbst " - Vidline" an (applicationDisplayName)
    setWindowTitle(pr_.sources.isEmpty() ? QString("Vidline") : QFileInfo(pr_.sources[0].path).fileName());
    for (const BtnSpec& s : btns_) {
        QString text = s.text ? T(s.text) : QString();
        s.b->setText(text.isEmpty() ? QString() : " " + text);
        QString tip = s.tip ? T(s.tip) : (s.text ? T(s.text) : QString());
        QString suffix = QString::fromLatin1(s.tipSuffix);
        if (suffix.startsWith('@')) {
            // Taste kommt aus den Einstellungen; feste "(Strg+Z)" im Text dann weglassen
            tip.remove(QRegularExpression("\\s*\\([^)]*\\)$"));
            const QString key = shortcutText(suffix.mid(1));
            suffix = key.isEmpty() ? QString() : " (" + key + ")";
        }
        s.b->setToolTip(tip + suffix);
    }
    lblHint_->setText(T("drop_hint"));
    lblHint_->adjustSize();
    lblInspHint_->setText(T("insp_hint"));
    lblSegment_->setText(T("segment"));
    lblSpeed_->setText(T("speed"));
    lblStart_->setText(T("start_s"));
    lblEnd_->setText(T("end_s"));
    lblPieceVol_->setText(T("volume"));
    lblSize_->setText(T("size"));
    lblPos_->setText(T("position"));
    lblRot_->setText(T("rotation"));
    btnResetTf_->setText(T("reset"));
    lblFont_->setText(T("font"));
    lblAspect_->setText(T("aspect"));
    aspectBox_->setItemText(0, T("original"));
    aspectBox_->setToolTip(T("aspect_tip"));
    lblColors_->setText(T("color"));
    lblShapeColor_->setText(T("color"));
    chkFill_->setText(T("fill"));
    chkBg_->setText(T("background"));
    for (QCheckBox* c : {chkPieceDenoise_, chkAudioDenoise_}) {
        c->setText(T("denoise"));
        c->setToolTip(T("denoise_tip"));
    }
    lblBinHint_->setText(bin_->count() ? T("bin_hint") : T("bin_empty"));
    textEdit_->setPlaceholderText(T("text_ph"));
    boostLbl_->setText(QString::fromUtf8("▶▶  ") + T("speed_boost"));
    lblHint_->move((view_->width() - lblHint_->width()) / 2, view_->height() / 2 - 10);
    refreshInspector();
    placeVideoControls();
    fitToolLabels();
    timeline_->update();
}

void MainWindow::fitToolLabels() {
    if (!tools_ || !tools_->isVisible()) return;
    for (const BtnSpec& s : btns_)
        if (s.text && s.b->objectName() == "tool") s.b->setText(" " + T(s.text));
    if (tools_->layout()->sizeHint().width() > tools_->width())
        for (const BtnSpec& s : btns_)
            if (s.text && s.b->objectName() == "tool") s.b->setText(QString());
}

void MainWindow::placeVideoControls() {
    if (!fsBtn_) return;
    fsBtn_->setIcon(makeIcon(fullscreen_ ? Ic::ExitFullscreen : Ic::Fullscreen, Qt::white));
    QString fsTip = T("fullscreen_tip");
    fsTip.remove(QRegularExpression("\\s*\\([^)]*\\)$"));
    fsBtn_->setToolTip(fsTip + " (" + shortcutText("fullscreen") + ")");
    updateFsBar();
    // Knopf über der Leiste, wenn sie sichtbar ist
    const int bottom = fsBar_ && fsBar_->isVisible() ? fsBar_->y() - 10 : view_->height() - 14;
    fsBtn_->move(view_->width() - fsBtn_->width() - 14, bottom - fsBtn_->height());
    fsBtn_->raise();
    if (fsHint_->isVisible()) {
        fsHint_->adjustSize();
        fsHint_->move((view_->width() - fsHint_->width()) / 2, 40);
    }
    boostLbl_->adjustSize();
    boostLbl_->move((view_->width() - boostLbl_->width()) / 2, 18);
}

// Beim Wechsel ins Vollbild kurz zeigen, wie man wieder herauskommt
void MainWindow::showFullscreenHint() { showVideoHint(T("fs_exit_hint"), 2600); }

void MainWindow::showVideoHint(const QString& text, int ms) {
    const int gen = ++hintGen_;
    fsHint_->setText(text);
    fsHint_->adjustSize();
    fsHint_->move((view_->width() - fsHint_->width()) / 2, 40);
    fsHint_->show();
    fsHint_->raise();
    auto* eff = new QGraphicsOpacityEffect(fsHint_);
    eff->setOpacity(1.0);
    fsHint_->setGraphicsEffect(eff);  // löscht den Effekt (samt Animation) eines vorherigen Hinweises
    QTimer::singleShot(ms, this, [this, gen, eff] {
        if (gen != hintGen_) return;  // inzwischen kam ein neuer Hinweis
        auto* anim = new QPropertyAnimation(eff, "opacity", eff);
        anim->setDuration(700);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, this, [this, gen] {
            if (gen == hintGen_) fsHint_->hide();
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

bool MainWindow::eventFilter(QObject* o, QEvent* e) {
    // Abspiel-Taste (Leertaste): kurz drücken = Pause/Abspielen, gedrückt halten = 2× bis zum Loslassen
    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && playKey_ &&
        (QApplication::activeWindow() == this || (silent_ && isVisible()))) {  // Tests laufen ohne aktives Fenster
        auto* ke = static_cast<QKeyEvent*>(e);
        const auto mods = ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier);
        if (ke->key() == playKey_ && !mods && !typingFocus()) {
            if (e->type() == QEvent::KeyPress) {
                if (!ke->isAutoRepeat() && !playKeyDown_) {
                    playKeyDown_ = true;
                    if (playingIntent() && !pr_.pieces.isEmpty()) holdTimer_->start();
                }
            } else if (!ke->isAutoRepeat() && playKeyDown_) {
                playKeyDown_ = false;
                holdTimer_->stop();
                if (boosted_) setBoost(false);
                else togglePlay();
            }
            return true;
        }
    }
    if (o == view_ && e->type() == QEvent::Resize) placeVideoControls();
    if (o == tlScroll_->viewport() && e->type() == QEvent::Resize)
        QTimer::singleShot(0, this, [this] { timeline_->relayout(); });
    return QMainWindow::eventFilter(o, e);
}

// Leertaste gehalten: doppelt so schnell, Ton läuft mit (wie bei YouTube)
void MainWindow::setBoost(bool on) {
    if (boosted_ == on) return;
    boosted_ = on;
    boost_ = on ? 2.0 : 1.0;
    if (!pending_ && cur_ >= 0 && cur_ < pr_.pieces.size()) player_->setPlaybackRate(pr_.pieces[cur_].speed * boost_);
    if (pending_ && cur_ >= 0 && cur_ < pr_.pieces.size()) pendingRate_ = pr_.pieces[cur_].speed * boost_;
    for (auto& [id, ap] : audioPlayers_) ap.p->setPlaybackRate(boost_);
    boostLbl_->setVisible(on);
    if (on) {
        placeVideoControls();
        boostLbl_->raise();
    }
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    fitView();
    QTimer::singleShot(0, this, [this] { fitToolLabels(); });
    lblHint_->move((view_->width() - lblHint_->width()) / 2, view_->height() / 2 - 10);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (exportProc_) exportProc_->kill();
    QMainWindow::closeEvent(e);
}

void MainWindow::fitView() {
    if (scene_->sceneRect().width() > 0) {
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
        view_->viewport()->update();
    }
}

// Video (evtl. mit anderer Größe als die Bildfläche) mittig einpassen
void MainWindow::fitVideoItem() { applyVideoTransform(); }

// Eingepasst, dann Größe / Verschiebung / Drehung des gerade laufenden Abschnitts
void MainWindow::applyVideoTransform() {
    QSizeF s = vitem_->nativeSize();
    if (!s.isValid() || s.isEmpty()) return;
    const Piece* pc = cur_ >= 0 && cur_ < pr_.pieces.size() ? &pr_.pieces[cur_] : nullptr;
    const double k = std::min(double(pr_.vw) / s.width(), double(pr_.vh) / s.height()) * (pc ? pc->scale : 1.0);
    const QSizeF fs(s.width() * k, s.height() * k);
    vitem_->setSize(fs);
    const double cx = pr_.vw / 2.0 + (pc ? pc->px * pr_.vw : 0), cy = pr_.vh / 2.0 + (pc ? pc->py * pr_.vh : 0);
    vitem_->setPos(cx - fs.width() / 2, cy - fs.height() / 2);
    vitem_->setTransformOriginPoint(fs.width() / 2, fs.height() / 2);
    vitem_->setRotation(pc ? pc->rot : 0);
}

// ---------------------------------------------------------------- Modi / Vollbild
void MainWindow::setEditMode(bool on) {
    if (on) ensureEditData();
    if (!on) setTool(0);
    applyLayoutVisibility();
    for (Overlay* ov : overlays_) ov->setAcceptedMouseButtons(showGuides() && !tool_ ? Qt::LeftButton : Qt::NoButton);
    for (Overlay* ov : overlays_) ov->update();
    if (!on) showBar();
}

void MainWindow::applyLayoutVisibility() {
    const bool edit = btnEdit_->isChecked();
    topbar_->setVisible(!fullscreen_ && edit);  // obere Leiste nur beim Bearbeiten
    // Abspielleiste: beim Anschauen direkt unter dem Video, beim Bearbeiten über Werkzeugen und Zeitleiste
    auto* eb = static_cast<QVBoxLayout*>(editBottom_->layout());
    if (edit && eb->indexOf(transport_) < 0) eb->insertWidget(0, transport_);
    if (!edit && rootLayout_->indexOf(transport_) < 0) rootLayout_->addWidget(transport_);
    transport_->setVisible(!fullscreen_);
    slider_->setVisible(!edit);  // Bearbeiten: die Zeitleiste ersetzt die Fortschrittsleiste
    seekGap_->setVisible(edit);
    editBottom_->setVisible(!fullscreen_ && edit);
    inspCard_->setVisible(!fullscreen_ && edit);
    mediaCard_->setVisible(!fullscreen_ && edit);
    if (fullscreen_) {
        rootLayout_->setContentsMargins(0, 0, 0, 0);
        rootLayout_->setSpacing(0);
        view_->setStyleSheet("QGraphicsView { border-radius: 0; border: none; }");
    } else {
        rootLayout_->setContentsMargins(14, 12, 14, 14);
        rootLayout_->setSpacing(10);
        view_->setStyleSheet(QString());
    }
    placeVideoControls();
    QTimer::singleShot(0, this, [this] { fitView(); timeline_->relayout(); placeVideoControls(); fitToolLabels(); });
}

void MainWindow::toggleFullscreen() { fullscreen_ ? exitFullscreen() : enterFullscreen(); }

void MainWindow::enterFullscreen() {
    fullscreen_ = true;
    wasMaximized_ = isMaximized();
    setTool(0);
    applyLayoutVisibility();
    showFullScreen();
    for (Overlay* ov : overlays_) { ov->setAcceptedMouseButtons(Qt::NoButton); ov->update(); }
    showBar();
    QTimer::singleShot(150, this, [this] { placeVideoControls(); showFullscreenHint(); });
}

void MainWindow::exitFullscreen() {
    fullscreen_ = false;
    view_->viewport()->unsetCursor();
    wasMaximized_ ? showMaximized() : showNormal();
    applyLayoutVisibility();
    setEditMode(btnEdit_->isChecked());
    fsHint_->hide();
    showBar();
    QTimer::singleShot(150, this, [this] { placeVideoControls(); });
}

// ---------------------------------------------------------------- Datei
void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

// Dateien ins Fenster ziehen: ohne Projekt wird die erste geöffnet. Beim Anschauen ersetzt ein Video/Ton auf dem
// Bild das aktuelle. Beim Bearbeiten landet alles im Medien-Panel; auf das Bild gezogen auch gleich an der
// Abspielposition (die Zeitleiste nimmt Dateien selbst an der Stelle an, wo man sie loslässt).
void MainWindow::dropEvent(QDropEvent* e) {
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) files << u.toLocalFile();
    if (files.isEmpty()) return;
    e->acceptProposedAction();
    const bool onVideo = view_->rect().contains(view_->mapFrom(this, e->position().toPoint()));
    auto playable = [](const QString& f) { return mediaKindOf(f) != MediaKind::Image; };
    if (pr_.pieces.isEmpty() || (!btnEdit_->isChecked() && onVideo)) {
        for (int i = 0; i < files.size(); ++i)
            if (playable(files[i])) { openFile(files.takeAt(i)); break; }
        for (const QString& f : files) addToBin(f);
        if (!files.isEmpty() && !pr_.pieces.isEmpty()) btnEdit_->setChecked(true);
        return;
    }
    if (!btnEdit_->isChecked()) btnEdit_->setChecked(true);
    for (const QString& f : files) {
        addToBin(f);
        if (onVideo) insertFromBin(f);
    }
}

void MainWindow::openDialog() {
    QString f = QFileDialog::getOpenFileName(this, T("open_video"), QString(),
                                             T("media_files") + " (*.mp4 *.mov *.mkv *.m4v *.avi *.webm *.wmv *.flv *.ts "
                                             "*.mp3 *.wav *.m4a *.aac *.flac *.ogg *.opus *.wma *.aiff);;*.*");
    if (!f.isEmpty()) openFile(f);
}

void MainWindow::openSettings() {
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::languageChanged, this, [this](const QString& c) {
        setLanguage(c);
        QSettings().setValue("language", c);
        retranslate();
    });
    connect(&dlg, &SettingsDialog::checkUpdates, this, [this] { updater_->check(true); });
    connect(&dlg, &SettingsDialog::themeChanged, this, [this](const QString& id) {
        setCurrentTheme(id);
        QSettings().setValue("theme", id);
        applyTheme();
    });
    connect(&dlg, &SettingsDialog::accentChanged, this, [this](const QString& color) {
        QSettings().setValue("accent", color);  // setAccentColor hat der Dialog schon gemacht
        applyTheme();
    });
    dlg.exec();
    applyShortcuts();
}

void MainWindow::openFile(const QString& path) {
    trace("openFile");
    player_->stop();
    for (auto& [id, ap] : audioPlayers_) { ap.p->stop(); ap.p->deleteLater(); ap.out->deleteLater(); }
    audioPlayers_.clear();
    setTool(0);
    setBoost(false);
    pr_ = Project();
    Source s;
    s.path = path;
    s.video = mediaKindOf(path) != MediaKind::Audio;
    pr_.sources.push_back(s);
    undo_.clear();
    redo_.clear();
    t_ = 0;
    cur_ = 0;
    selPiece_ = -1;
    selItem_ = 0;
    selAudio_ = 0;
    loading_ = true;
    canvasSet_ = false;
    loadedSrc_ = 0;
    pending_ = false;
    scrubbing_ = false;
    fsHint_->hide();
    lblHint_->hide();
    setWindowTitle(QFileInfo(path).fileName());
    // Tondatei: wie ein Musik-Player (Cover verschwommen im Hintergrund, klein unten links, Titel daneben)
    view_->setAmbient(QImage());  // kein Licht vom vorherigen Video
    if (s.video) {
        audioShown_.clear();
        view_->setAudioInfo(QString());
    } else {
        showAudioInfo(path);
    }
    if (!s.video) {
        canvasSet_ = true;
        canvasBg_->setBrush(Qt::NoBrush);  // die 16:9-Fläche bleibt durchsichtig, dahinter liegt das Cover
        pr_.natW = 1920;
        pr_.natH = 1080;
        applyCanvas();
    }
    player_->setSource(QUrl());  // dieselbe Datei nochmal öffnen: sonst lädt der Player sie nicht neu
    player_->setSource(QUrl::fromLocalFile(path));
    player_->pause();  // erstes Bild sofort anzeigen
    // Erst das Video, alles fürs Bearbeiten (Wellenform, Vorschaubilder) später bzw. erst im Bearbeiten-Modus
    addToBin(path);
    if (editData_) startWaves(path);
    rebuildOverlays();
    refreshInspector();
    timeline_->relayout();
    barShown_ = false;
    showBar();
}

void MainWindow::onDuration(qint64 ms) {
    if (ms > 0 && loading_) {
        loading_ = false;
        pr_.sources[0].duration = ms / 1000.0;
        pr_.pieces = {Piece{0, 0.0, ms / 1000.0, 1.0}};
        timeline_->relayout();
        updateUi();
        // Vorschaubilder (für das Bild beim Spulen) erst, wenn das Video schon läuft
        const QString path = pr_.sources[0].path;
        QTimer::singleShot(editData_ ? 100 : 1500, this, [this, path] {
            if (!pr_.sources.isEmpty() && pr_.sources[0].path == path) startThumbs(0);
        });
    }
}

// Alles, was nur der Editor braucht, beim ersten Wechsel ins Bearbeiten nachholen
void MainWindow::ensureEditData() {
    if (!editData_) {
        editData_ = true;
        const QStringList pending = pendingBinThumbs_;
        pendingBinThumbs_.clear();
        for (const QString& p : pending) {
            for (int i = 0; i < bin_->count(); ++i)
                if (bin_->item(i)->data(Qt::UserRole).toString() == p) { delete bin_->takeItem(i); break; }
            addToBin(p);
        }
    }
    for (int i = 0; i < pr_.sources.size(); ++i) {
        startWaves(pr_.sources[i].path);
        startThumbs(i);
    }
}

void MainWindow::ensureFonts() {
    if (fontsReady_) return;
    fontsReady_ = true;
    registerSpecialFonts();
    for (const QString& f : specialFonts()) fontBox_->addItem(QString::fromUtf8("★ ") + f, f);
    fontBox_->insertSeparator(fontBox_->count());
    for (const QString& f : QFontDatabase::families())
        if (!specialFonts().contains(f) && !f.startsWith('@')) fontBox_->addItem(f, f);
}

void MainWindow::onNativeSize(const QSizeF& s) {
    if (!s.isValid() || s.isEmpty()) return;
    if (!canvasSet_) {
        canvasSet_ = true;
        canvasBg_->setBrush(Qt::black);
        pr_.natW = int(s.width());
        pr_.natH = int(s.height());
        applyCanvas();
        return;
    }
    fitVideoItem();
    fitView();
}

// Bildfläche aus Bildformat + erstem Video; Video wird darin eingepasst (Rest schwarz)
void MainWindow::applyCanvas() {
    const QSize c = canvasSize(pr_.aspect, pr_.natW, pr_.natH);
    if (c.width() != pr_.vw || c.height() != pr_.vh) {
        pr_.vw = c.width();
        pr_.vh = c.height();
        // Bilder behalten ihr Seitenverhältnis, alles bleibt innerhalb der Fläche
        for (Item& it : pr_.items) {
            if (it.kind == Item::Image) {
                it.h = it.w * pr_.vw * it.ar / pr_.vh;
                if (it.h > 1) { it.w /= it.h; it.h = 1; }
            }
            it.w = std::min(it.w, 1.0);
            it.h = std::min(it.h, 1.0);
            it.x = std::clamp(it.x, 0.0, 1.0 - it.w);
            it.y = std::clamp(it.y, 0.0, 1.0 - it.h);
        }
    }
    scene_->setSceneRect(0, 0, pr_.vw, pr_.vh);
    canvasBg_->setRect(0, 0, pr_.vw, pr_.vh);
    if (aspectBox_) aspectBox_->setCurrentIndex(pr_.aspect);
    rebuildOverlays();
    fitVideoItem();
    fitView();
}

void MainWindow::setAspect(int aspect) {
    aspect = std::clamp(aspect, 0, kAspectCount - 1);
    if (aspect == pr_.aspect || pr_.sources.isEmpty()) {
        if (aspectBox_) aspectBox_->setCurrentIndex(pr_.aspect);
        return;
    }
    pushUndo();
    pr_.aspect = aspect;
    applyCanvas();
    modelEdited();
}

void MainWindow::onStatus(QMediaPlayer::MediaStatus st) {
    if (st == QMediaPlayer::LoadedMedia || st == QMediaPlayer::BufferedMedia) {
        onNativeSize(vitem_->nativeSize());
        if (pending_) {
            pending_ = false;
            player_->setPlaybackRate(pendingRate_);
            applyPlayerVolume();
            player_->setPosition(qint64(pendingPos_ * 1000));
            if (pendingPlay_) player_->play(); else player_->pause();
        }
    }
}

// ---------------------------------------------------------------- Bildfluss / Ambient
QImage MainWindow::frameImage() {
    if (!frameImgValid_) {
        frameImg_ = frame_.isValid() ? frame_.toImage() : QImage();
        frameImgValid_ = true;
    }
    return frameImg_;
}

void MainWindow::onFrame(const QVideoFrame& f) {
    if (!firstFrame_ && f.isValid()) {
        firstFrame_ = true;
        trace("first frame");
    }
    if (scrubInFlight_) {  // angefordertes Bild beim Spulen ist da: anhalten, gleich das nächste
        scrubInFlight_ = false;
        if (player_->playbackState() == QMediaPlayer::PlayingState) player_->pause();
        QTimer::singleShot(0, this, &MainWindow::pumpScrub);
    }
    frame_ = f;
    frameImgValid_ = false;
    bool blurVisible = false;
    for (Overlay* ov : overlays_) {
        Item* it = pr_.item(ov->itemId());
        if (it && it->kind == Item::Blur && ov->isVisible()) { ov->update(); blurVisible = true; }
    }
    (void)blurVisible;
    // Ambient-Licht nur, wenn die Ränder frei sind
    const QSize vp = view_->viewport()->size();
    const double viewAr = double(vp.width()) / std::max(1, vp.height());
    const double vidAr = double(pr_.vw) / std::max(1, pr_.vh);
    if (std::abs(viewAr - vidAr) > 0.02 && ambientClock_.elapsed() > 90) {
        ambientClock_.restart();
        QImage im = frameImage();
        if (!im.isNull()) view_->setAmbient(im.scaled(16, 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
}

// ---------------------------------------------------------------- Wiedergabe
void MainWindow::onState(QMediaPlayer::PlaybackState st) {
    if (st == QMediaPlayer::PlayingState) timer_->start();
    else timer_->stop();
    updatePlayIcon();
    updateAudio(true);
    if (st != QMediaPlayer::PlayingState && boosted_ && !scrubbing_) setBoost(false);
}

void MainWindow::togglePlay() {
    if (pr_.pieces.isEmpty()) return;
    if (player_->playbackState() == QMediaPlayer::PlayingState) {
        player_->pause();
        if (osd_) osd_->flash(Osd::Pause);
        showBar();  // pausiert: Leiste kurz zeigen (verschwindet aber auch pausiert wieder)
    } else {
        if (t_ >= pr_.total() - 0.05) seek(0);
        player_->play();
        if (osd_) osd_->flash(Osd::Play);
    }
}

// Abschnitt aktivieren; bei anderer Quelldatei wird nachgeladen
void MainWindow::activate(int i, double srcPos, bool play) {
    const bool changed = cur_ != i;
    cur_ = i;
    if (changed) applyVideoTransform();
    const Piece& p = pr_.pieces[i];
    if (p.src != loadedSrc_) {
        loadedSrc_ = p.src;
        pending_ = true;
        pendingPlay_ = play;
        pendingPos_ = srcPos;
        pendingRate_ = p.speed * boost_;
        if (pr_.sources[p.src].video) {
            audioShown_.clear();
            view_->setAudioInfo(QString());
        } else {
            showAudioInfo(pr_.sources[p.src].path);
        }
        player_->setSource(QUrl::fromLocalFile(pr_.sources[p.src].path));
        player_->pause();
        ++posSets_;
        return;
    }
    if (pending_) {  // Ladevorgang läuft noch: Ziel aktualisieren
        pendingPos_ = srcPos;
        pendingRate_ = p.speed * boost_;
        pendingPlay_ = play;
        return;
    }
    player_->setPlaybackRate(p.speed * boost_);
    applyPlayerVolume();
    // Nur springen, wenn es wirklich woanders hingeht; bis der Player angekommen ist, liefert er noch die alte Position
    if (std::abs(player_->position() / 1000.0 - srcPos) > 0.04) {
        // Beim Abspielen kurz stumm: sonst spielt der Puffer noch Ton von der alten Stelle ("komische Geräusche")
        if (player_->playbackState() == QMediaPlayer::PlayingState) muteBriefly();
        player_->setPosition(qint64(srcPos * 1000));
        ++posSets_;
        seeking_ = true;
        seekSrc_ = srcPos;
        seekClock_.restart();
    }
    if (play && player_->playbackState() != QMediaPlayer::PlayingState) player_->play();
}

void MainWindow::applyPlayerVolume() {
    const double pv = cur_ >= 0 && cur_ < pr_.pieces.size() ? pr_.pieces[cur_].volume : 1.0;
    audio_->setVolume(float(std::clamp(pv * masterVol_, 0.0, 1.0)));
}

void MainWindow::muteBriefly() {
    audio_->setMuted(true);
    unmuteTimer_->start();
}

// Ziehen beginnt: anhalten und stumm. Vorher liefen die Sprünge beim Abspielen weiter - jeder Sprung spielte ein
// Stück Ton, und das Video lief zwischen den Sprüngen ein Stück weiter.
void MainWindow::beginScrub() {
    if (scrubbing_ || pr_.pieces.isEmpty()) return;
    scrubResume_ = player_->playbackState() == QMediaPlayer::PlayingState || (pending_ && pendingPlay_);
    scrubbing_ = true;
    audio_->setMuted(true);
    unmuteTimer_->stop();
    if (player_->playbackState() == QMediaPlayer::PlayingState) player_->pause();
    if (pending_) pendingPlay_ = false;
}

// Ziehen vorbei (Stelle ist schon angesprungen): weiter wie vorher, Ton nach einem Moment wieder an
void MainWindow::endScrub() {
    if (!scrubbing_) return;
    scrubbing_ = false;
    scrubInFlight_ = false;
    scrubIssued_ = -1;
    seekTimer_->stop();
    if (!scrubResume_ && player_->playbackState() == QMediaPlayer::PlayingState) player_->pause();
    if (scrubResume_) {
        if (pending_) pendingPlay_ = true;
        else if (t_ < pr_.total() - 0.05) player_->play();
    }
    scrubResume_ = false;
    muteBriefly();
}

// Beim Ziehen: Zeit/Anzeige sofort, echte Sprünge höchstens alle 120 ms (sonst staut sich der Decoder)
void MainWindow::scrub(double t) {
    if (pr_.pieces.isEmpty()) return;
    beginScrub();
    scrubTarget_ = std::clamp(t, 0.0, pr_.total());
    t_ = scrubTarget_;
    updateUi();
    pumpScrub();
}

// Beim Spulen jedes Bild zeigen, an dem man vorbeikommt: Sprung anfordern, auf das Bild warten (onFrame),
// dann gleich zur aktuellen Mausposition weiter. Ein angefordertes Bild wird nie abgebrochen (bei Videos mit
// seltenen Schlüsselbildern dauert das Dekodieren länger); kommt es gar nicht, geht es nach 800 ms weiter.
void MainWindow::pumpScrub() {
    if (!scrubbing_ || pr_.pieces.isEmpty()) return;
    if (scrubInFlight_ && scrubFlight_.elapsed() < 800) {
        if (!seekTimer_->isActive()) seekTimer_->start(800 - int(scrubFlight_.elapsed()));
        return;
    }
    scrubInFlight_ = false;
    if (std::abs(scrubTarget_ - scrubIssued_) < 0.005) return;  // genau dieses Bild ist schon da
    scrubIssued_ = scrubTarget_;
    const int before = posSets_;
    const double target = scrubTarget_;
    seek(target);  // stoppt seekTimer_
    if (posSets_ != before) {  // Player springt wirklich: auf das Bild warten
        scrubInFlight_ = true;
        scrubFlight_.restart();
        seekTimer_->start(800);
        // Pausiert liefert Qts Player nach einem Sprung nur beim ersten Mal ein neues Bild - danach bleibt das
        // alte stehen. Darum kurz (stumm) anspielen; sobald das Bild da ist, hält onFrame wieder an.
        if (!pending_ && player_->playbackState() != QMediaPlayer::PlayingState) player_->play();
    }
}

// Fortschritt (unter dem Video und im Vollbild): Ziehen pausiert, macht stumm und zeigt jedes Bild, an dem man
// vorbeikommt (dazu Vorschaubild + Zeit über dem Regler); beim Loslassen geht es weiter. Ein Klick springt sofort.
void MainWindow::wireSeekSlider(QSlider* s) {
    connect(s, &QSlider::sliderPressed, this, [this] { beginScrub(); });
    connect(s, &QSlider::sliderMoved, this, [this, s](int v) {
        if (!s->isSliderDown()) return seek(v / 10000.0 * pr_.total());
        showScrubPreview(v, s);
        scrub(v / 10000.0 * pr_.total());
    });
    connect(s, &QSlider::sliderReleased, this, [this, s] {
        scrubPrev_->hide();
        seek(s->value() / 10000.0 * pr_.total());
        endScrub();
        showBar();
    });
}

// Vorschaubild + Zeit über dem Regler (sofort, ohne auf den Decoder zu warten)
void MainWindow::showScrubPreview(int v, QSlider* from) {
    if (pr_.pieces.isEmpty()) return;
    const double t = v / 10000.0 * pr_.total();
    double src = 0;
    const int i = pr_.locate(t, &src);
    QImage thumb;
    if (const ThumbSet* ts = thumbs(pr_.pieces[i].src); ts && !ts->imgs.isEmpty())
        thumb = ts->imgs[std::clamp(int(src / ts->step), 0, int(ts->imgs.size()) - 1)]
                    .scaledToHeight(100, Qt::SmoothTransformation);
    const QString label = fmtTime(t);
    QFont f = font();
    f.setBold(true);
    const QFontMetrics fm(f);
    const int w = std::max(thumb.isNull() ? 0 : thumb.width() + 8, fm.horizontalAdvance(label) + 20);
    const int h = (thumb.isNull() ? 0 : thumb.height() + 4) + fm.height() + 12;
    QPixmap pm(w * devicePixelRatio(), h * devicePixelRatio());
    pm.setDevicePixelRatio(devicePixelRatio());
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(255, 255, 255, 60));
    p.setBrush(QColor(12, 13, 18, 230));
    p.drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10, 10);
    if (!thumb.isNull()) p.drawImage(QPoint((w - thumb.width()) / 2, 4), thumb);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, h - fm.height() - 8, w, fm.height() + 4), Qt::AlignCenter, label);
    p.end();
    scrubPrev_->setPixmap(pm);
    scrubPrev_->resize(w, h);
    const QPoint base = from->mapTo(this, QPoint(0, 0));
    const int hx = base.x() + 7 + int(double(v) / from->maximum() * (from->width() - 14));
    scrubPrev_->move(std::clamp(hx - w / 2, 4, width() - w - 4), base.y() - h - 8);
    scrubPrev_->show();
    scrubPrev_->raise();
}

void MainWindow::seek(double t) {
    if (pr_.pieces.isEmpty()) return;
    seekTimer_->stop();  // ein direkter Sprung ersetzt einen noch ausstehenden gedrosselten
    t = std::clamp(t, 0.0, pr_.total());
    lastSeekT_ = t;
    double src = 0;
    int i = pr_.locate(t, &src);
    activate(i, src, player_->playbackState() == QMediaPlayer::PlayingState);
    t_ = t;
    updateUi();
    updateAudio(true);
}

// Abschnittswechsel während der Wiedergabe: springt zum nächsten Abschnitt und setzt das Tempo
void MainWindow::tick() {
    if (pr_.pieces.isEmpty() || pending_ || scrubbing_) return;  // beim Spulen bestimmt die Maus die Stelle
    cur_ = std::min<int>(cur_, pr_.pieces.size() - 1);
    const Piece* p = &pr_.pieces[cur_];
    double src = player_->position() / 1000.0;
    if (seeking_) {  // Sprung noch unterwegs: alte Position nicht anzeigen
        if (std::abs(src - seekSrc_) < 0.75 || seekClock_.elapsed() > 2000) seeking_ = false;
        else return;
    }
    bool ended = player_->mediaStatus() == QMediaPlayer::EndOfMedia;
    if (src >= p->end - 0.03 || ended) {
        if (cur_ + 1 < pr_.pieces.size()) {
            activate(cur_ + 1, pr_.pieces[cur_ + 1].start, true);
            t_ = pr_.outStart(cur_);
            updateUi();
            updateAudio(false);
            return;
        }
        player_->pause();
        t_ = pr_.total();
        updateUi();
        showBar();  // zu Ende: Leiste zeigen
        return;
    }
    t_ = pr_.outStart(cur_) + std::max(0.0, src - p->start) / p->speed;
    updateUi();
    updateAudio(false);
}

void MainWindow::updateUi() {
    double total = pr_.total();
    // Beim Ziehen an der Leiste zeigt die Zeit genau die Mausstelle (das Bild kommt einen Moment später)
    const QSlider* drag = slider_->isSliderDown() ? slider_ : fsSlider_ && fsSlider_->isSliderDown() ? fsSlider_ : nullptr;
    const double shown = drag ? drag->value() / 10000.0 * total : t_;
    lblTime_->setText(fmtTime(shown) + " / " + fmtTime(total));
    if (!slider_->isSliderDown()) {
        QSignalBlocker b(slider_);
        slider_->setValue(total > 0 ? int(t_ / total * 10000) : 0);
    }
    if (fsBar_ && fsBar_->isVisible()) {
        fsTime_->setText(fmtTime(shown) + " / " + fmtTime(total));
        if (!fsSlider_->isSliderDown()) {
            QSignalBlocker b(fsSlider_);
            fsSlider_->setValue(total > 0 ? int(t_ / total * 10000) : 0);
        }
    }
    for (Overlay* ov : overlays_) {
        if (Item* it = pr_.item(ov->itemId())) ov->setVisible(it->t0 <= t_ && t_ < it->t1);
    }
    timeline_->update();
}

// ---------------------------------------------------------------- Ton-Spuren (Vorschau)
void MainWindow::syncAudioPlayers() {
    for (auto it = audioPlayers_.begin(); it != audioPlayers_.end();) {
        if (!pr_.audio(it->first)) {
            it->second.p->stop();
            it->second.p->deleteLater();
            it->second.out->deleteLater();
            it = audioPlayers_.erase(it);
        } else {
            ++it;
        }
    }
    for (const AudioClip& a : pr_.audios) {
        if (audioPlayers_.count(a.id)) continue;
        AudioPlayer ap{new QMediaPlayer(this), new QAudioOutput(this)};
        ap.p->setAudioOutput(ap.out);
        ap.out->setMuted(silent_);
        ap.p->setPlaybackRate(boost_);
        ap.p->setSource(QUrl::fromLocalFile(a.path));
        audioPlayers_[a.id] = ap;
    }
}

void MainWindow::updateAudio(bool force) {
    const bool playing = player_->playbackState() == QMediaPlayer::PlayingState && !scrubbing_;
    for (auto& [id, ap] : audioPlayers_) {
        AudioClip* a = pr_.audio(id);
        if (!a) continue;
        ap.out->setVolume(float(std::clamp(a->volume * masterVol_, 0.0, 1.0)));
        const bool inRange = t_ >= a->t0 && t_ < a->t0 + a->dur;
        if (playing && inRange) {
            qint64 want = qint64((a->srcStart + (t_ - a->t0)) * 1000);
            if (ap.p->playbackState() != QMediaPlayer::PlayingState) {
                ap.p->setPosition(want);
                ap.p->play();
            } else if (force || std::llabs(ap.p->position() - want) > 300) {
                ap.p->setPosition(want);
            }
        } else if (ap.p->playbackState() == QMediaPlayer::PlayingState) {
            ap.p->pause();
        }
    }
}

// ---------------------------------------------------------------- Vorschaubilder
const ThumbSet* MainWindow::thumbs(int srcIndex) const {
    if (srcIndex < 0 || srcIndex >= pr_.sources.size()) return nullptr;
    auto it = thumbSets_.find(pr_.sources[srcIndex].path);
    return it == thumbSets_.end() ? nullptr : it->second.get();
}

void MainWindow::startThumbs(int si) {
    if (si < 0 || si >= pr_.sources.size()) return;
    const Source& s = pr_.sources[si];
    if (thumbSets_.count(s.path) || s.duration <= 0) return;
    auto ts = std::make_shared<ThumbSet>();
    ts->step = std::max(0.5, s.duration / 110.0);
    thumbSets_[s.path] = ts;
    auto dir = std::make_unique<QTemporaryDir>();
    const QString d = dir->path();
    thumbDirs_.push_back(std::move(dir));
    auto* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p, d, ts] {
        const QStringList files = QDir(d).entryList({"t*.jpg"}, QDir::Files, QDir::Name);
        for (const QString& f : files) {
            QImage im(QDir(d).filePath(f));
            if (!im.isNull()) ts->imgs.append(im);
        }
        p->deleteLater();
        timeline_->update();
    });
    p->start(ffmpegPath(), {"-hide_banner", "-v", "error", "-skip_frame", "nokey", "-i", s.path, "-an", "-sn",
                            "-vf", QString("fps=1/%1,scale=-2:120").arg(ts->step, 0, 'f', 3), "-q:v", "3",
                            d + "/t%05d.jpg"});
}

void MainWindow::ensureProbed() {
    for (Source& s : pr_.sources) {
        if (s.probed) continue;
        MediaInfo m = probeMedia(s.path);
        s.fps = m.fps;
        s.hasAudio = m.hasAudio;
        if (s.duration <= 0) s.duration = m.duration;
        if (!s.video || m.w <= 0) {  // Tondatei: Bild bleibt schwarz, für den Export wie ein 30-fps-Video behandeln
            s.video = false;
            if (s.fps <= 0) s.fps = 30;
        }
        s.probed = true;
    }
}

// ---------------------------------------------------------------- Bearbeiten
void MainWindow::pushUndo() {
    undo_.push_back(pr_.snapshot());
    if (undo_.size() > 100) undo_.erase(undo_.begin());
    redo_.clear();
}

void MainWindow::doUndo() {
    if (undo_.empty()) return;
    redo_.push_back(pr_.snapshot());
    pr_.restore(undo_.back());
    undo_.pop_back();
    afterRestore();
}

void MainWindow::doRedo() {
    if (redo_.empty()) return;
    undo_.push_back(pr_.snapshot());
    pr_.restore(redo_.back());
    redo_.pop_back();
    afterRestore();
}

void MainWindow::afterRestore() {
    selItem_ = 0;
    selAudio_ = 0;
    selPiece_ = std::min<int>(selPiece_, pr_.pieces.size() - 1);
    if (canvasSet_) applyCanvas();
    modelEdited();
}

void MainWindow::modelEdited(bool keepTime) {
    t_ = std::clamp(t_, 0.0, pr_.total());
    if (!keepTime) {
        rebuildOverlays();
        syncAudioPlayers();
    }
    timeline_->relayout();
    refreshInspector();
    if (!keepTime) seek(t_);
    applyPlayerVolume();
    applyVideoTransform();
    updateUi();
    updateAudio(false);
}

void MainWindow::rebuildOverlays() {
    for (Overlay* ov : overlays_) {
        scene_->removeItem(ov);
        delete ov;
    }
    overlays_.clear();
    for (const Item& it : pr_.items) {
        auto* ov = new Overlay(it.id, this);
        ov->setAcceptedMouseButtons(showGuides() ? Qt::LeftButton : Qt::NoButton);
        scene_->addItem(ov);
        overlays_.push_back(ov);
    }
    updateUi();
}

void MainWindow::selectPiece(int i) {
    selPiece_ = i;
    if (i >= 0) { selItem_ = 0; selAudio_ = 0; }
    refreshInspector();
    timeline_->update();
    for (Overlay* ov : overlays_) ov->update();
}

void MainWindow::selectItem(int id) {
    selItem_ = id;
    if (id) { selPiece_ = -1; selAudio_ = 0; }
    refreshInspector();
    timeline_->update();
    for (Overlay* ov : overlays_) ov->update();
}

void MainWindow::selectAudio(int id) {
    selAudio_ = id;
    if (id) { selPiece_ = -1; selItem_ = 0; }
    refreshInspector();
    timeline_->update();
    for (Overlay* ov : overlays_) ov->update();
}

void MainWindow::refreshInspector() {
    Item* it = selItem_ ? pr_.item(selItem_) : nullptr;
    AudioClip* au = selAudio_ ? pr_.audio(selAudio_) : nullptr;
    // Neue Auswahl -> Eigenschaften zeigen
    const QString sel = QString("%1/%2/%3").arg(selPiece_).arg(selItem_).arg(selAudio_);
    if (sel != lastSel_) {
        lastSel_ = sel;
        textUndo_ = false;
    }
    if (it && it->kind == Item::Text) ensureFonts();
    textBox_->setVisible(it && it->kind == Item::Text);
    shapeBox_->setVisible(it && it->graphic());
    chkFill_->setVisible(it && it->kind == Item::Shape && (it->shape == Item::Rect || it->shape == Item::Ellipse));
    chkAudioDenoise_->setVisible(au != nullptr);
    sliderStr_->setVisible(true);
    lblStr_->setVisible(true);
    if (au) {
        insp_->setCurrentIndex(2);
        lblItem_->setText(T("audio_el"));
        { QSignalBlocker b(spT0_); spT0_->setValue(au->t0); }
        { QSignalBlocker b(spT1_); spT1_->setValue(au->t0 + au->dur); }
        lblStr_->setText(T("volume") + QString("  %1 %").arg(qRound(au->volume * 100)));
        QSignalBlocker b(sliderStr_);
        sliderStr_->setRange(0, 600);  // bis 600 %
        sliderStr_->setValue(int(au->volume * 100));
        { QSignalBlocker c(chkAudioDenoise_); chkAudioDenoise_->setChecked(au->denoise); }
    } else if (it) {
        insp_->setCurrentIndex(2);
        static const char* shapeKeys[] = {"shape_rect", "shape_ellipse", "shape_arrow", "shape_line"};
        lblItem_->setText(it->kind == Item::Image ? T("image") : it->kind == Item::Text ? T("text")
                          : it->kind == Item::Draw ? T("drawing")
                          : it->kind == Item::Shape ? T(shapeKeys[std::clamp(it->shape, 0, 3)]) : T("blur_area"));
        { QSignalBlocker b(spT0_); spT0_->setValue(it->t0); }
        { QSignalBlocker b(spT1_); spT1_->setValue(it->t1); }
        lblStr_->setText(it->kind == Item::Blur ? T("strength") : it->graphic() ? T("stroke") : T("size"));
        {
            QSignalBlocker b(sliderStr_);
            if (it->graphic()) {
                sliderStr_->setRange(1, 40);
                sliderStr_->setValue(qRound(it->stroke));
            } else {
                sliderStr_->setRange(5, 100);
                sliderStr_->setValue(it->kind == Item::Blur ? int(it->strength) : int(it->w * 100));
            }
        }
        if (it->graphic()) {
            btnShapeColor_->setStyleSheet(QString("QPushButton { background: %1; border: 1px solid #888; border-radius: 5px; }")
                                              .arg(QColor::fromRgba(it->color).name(QColor::HexArgb)));
            QSignalBlocker b(chkFill_);
            chkFill_->setChecked(it->fill);
        }
        if (it->kind == Item::Text) {
            if (textEdit_->toPlainText() != it->text) { QSignalBlocker b(textEdit_); textEdit_->setPlainText(it->text); }
            { QSignalBlocker b(fontBox_); fontBox_->setCurrentIndex(std::max(0, fontBox_->findData(it->font))); }
            { QSignalBlocker b(chkBg_); chkBg_->setChecked(it->bg); }
            btnBgColor_->setEnabled(it->bg);
            auto swatch = [](QPushButton* btn, unsigned argb) {
                btn->setStyleSheet(QString("QPushButton { background: %1; border: 1px solid #888; border-radius: 5px; }")
                                       .arg(QColor::fromRgba(argb).name(QColor::HexArgb)));
            };
            swatch(btnColor_, it->color);
            swatch(btnBgColor_, it->bgColor);
        }
    } else if (selPiece_ >= 0 && selPiece_ < pr_.pieces.size()) {
        const Piece& p = pr_.pieces[selPiece_];
        insp_->setCurrentIndex(1);
        { QSignalBlocker b(spSpeed_); spSpeed_->setValue(p.speed); }
        { QSignalBlocker b(sliderSpeed_); sliderSpeed_->setValue(nearestSpeedStep(p.speed)); }
        { QSignalBlocker b(sliderPieceVol_); sliderPieceVol_->setValue(qRound(p.volume * 100)); }
        { QSignalBlocker b(chkPieceDenoise_); chkPieceDenoise_->setChecked(p.denoise); }
        chkPieceDenoise_->setVisible(pr_.sources.value(p.src).hasAudio);
        lblPieceVol_->setText(T("volume") + QString("  %1 %").arg(qRound(p.volume * 100)));
        { QSignalBlocker b(sliderScale_); sliderScale_->setValue(qRound(p.scale * 100)); }
        { QSignalBlocker b(spScale_); spScale_->setValue(p.scale * 100); }
        { QSignalBlocker b(spPosX_); spPosX_->setValue(p.px * 100); }
        { QSignalBlocker b(spPosY_); spPosY_->setValue(p.py * 100); }
        { QSignalBlocker b(spRot_); spRot_->setValue(p.rot); }
        lblPiece_->setText(T("source_range").arg(fmtTime(p.start), fmtTime(p.end)) + "\n" +
                           T("result_dur").arg(fmtTime(p.outDur())));
    } else {
        insp_->setCurrentIndex(0);
    }
}

void MainWindow::setSpeed(double s) {
    if (selPiece_ >= 0 && selPiece_ < pr_.pieces.size() && pr_.pieces[selPiece_].speed != s) {
        pushUndo();
        pr_.pieces[selPiece_].speed = s;
        modelEdited();
    }
}

void MainWindow::setPieceVolume(int v) {
    if (selPiece_ < 0 || selPiece_ >= pr_.pieces.size()) return;
    pr_.pieces[selPiece_].volume = v / 100.0;
    lblPieceVol_->setText(T("volume") + QString("  %1 %").arg(v));
    applyPlayerVolume();
    timeline_->update();
}

void MainWindow::itemTimesChanged() {
    double total = pr_.total();
    if (AudioClip* a = selAudio_ ? pr_.audio(selAudio_) : nullptr) {
        pushUndo();
        a->t0 = std::max(0.0, spT0_->value());
        a->dur = std::clamp(spT1_->value() - a->t0, 0.1, a->fileDur - a->srcStart);
        modelEdited(true);
        refreshInspector();
        return;
    }
    Item* it = selItem_ ? pr_.item(selItem_) : nullptr;
    if (!it) return;
    pushUndo();
    it->t0 = std::min(spT0_->value(), total);
    it->t1 = std::max(it->t0 + 0.1, std::min(spT1_->value(), total));
    modelEdited(true);
}

void MainWindow::sliderChanged(int v) {
    if (AudioClip* a = selAudio_ ? pr_.audio(selAudio_) : nullptr) {
        a->volume = v / 100.0;
        lblStr_->setText(T("volume") + QString("  %1 %").arg(v));
        updateAudio(false);
        return;
    }
    Item* it = selItem_ ? pr_.item(selItem_) : nullptr;
    if (!it) return;
    if (it->kind == Item::Blur) {
        it->strength = v;
        for (Overlay* ov : overlays_) ov->update();
    } else if (it->graphic()) {
        it->stroke = v;
        for (Overlay* ov : overlays_) ov->update();
    } else if (it->kind == Item::Text) {
        const double k = (v / 100.0) / std::max(0.01, it->w);
        it->w = v / 100.0;
        it->h = std::min(1.0, it->h * k);
        it->x = std::min(it->x, std::max(0.0, 1 - it->w));
        it->y = std::min(it->y, std::max(0.0, 1 - it->h));
        rebuildOverlays();
    } else {
        it->w = v / 100.0;
        it->h = it->w * pr_.vw * it->ar / pr_.vh;
        it->x = std::min(it->x, std::max(0.0, 1 - it->w));
        it->y = std::min(it->y, std::max(0.0, 1 - it->h));
        rebuildOverlays();
    }
}

void MainWindow::deleteSelected() {
    if (selAudio_) {
        for (int i = 0; i < pr_.audios.size(); ++i)
            if (pr_.audios[i].id == selAudio_) {
                pushUndo();
                pr_.audios.removeAt(i);
                selAudio_ = 0;
                modelEdited();
                return;
            }
    }
    for (int i = 0; i < pr_.items.size(); ++i) {
        if (pr_.items[i].id == selItem_) {
            pushUndo();
            pr_.items.removeAt(i);
            selItem_ = 0;
            modelEdited();
            return;
        }
    }
}

// Teilen: nur das ausgewählte Element (Video-Abschnitt, Ton oder Element);
// ist nichts ausgewählt, wird alles geteilt, was an der Abspielposition liegt.
void MainWindow::doSplit() {
    if (pr_.pieces.isEmpty()) return;
    const double t = t_;
    auto splitVideo = [&]() {
        double src = 0;
        const int i = pr_.locate(t, &src);
        const Piece p = pr_.pieces[i];
        if (src - p.start < kMinPiece || p.end - src < kMinPiece) return false;
        pr_.pieces[i].end = src;
        Piece rest = p;
        rest.start = src;
        pr_.pieces.insert(i + 1, rest);
        return true;
    };
    auto splitAudio = [&](int id) {
        for (int i = 0; i < pr_.audios.size(); ++i) {
            AudioClip& a = pr_.audios[i];
            if (a.id != id) continue;
            if (t - a.t0 < kMinPiece || a.t0 + a.dur - t < kMinPiece) return false;
            AudioClip b = a;
            b.id = pr_.nextId++;
            b.t0 = t;
            b.srcStart = a.srcStart + (t - a.t0);
            b.dur = a.t0 + a.dur - t;
            a.dur = t - a.t0;
            pr_.audios.insert(i + 1, b);
            return true;
        }
        return false;
    };
    auto splitItem = [&](int id) {
        for (int i = 0; i < pr_.items.size(); ++i) {
            Item& it = pr_.items[i];
            if (it.id != id) continue;
            if (t - it.t0 < kMinPiece || it.t1 - t < kMinPiece) return false;
            Item b = it;
            b.id = pr_.nextId++;
            b.t0 = t;
            it.t1 = t;
            pr_.items.insert(i + 1, b);
            return true;
        }
        return false;
    };
    pushUndo();
    bool changed = false;
    if (selAudio_) {
        changed = splitAudio(selAudio_);
    } else if (selItem_) {
        changed = splitItem(selItem_);
    } else if (selPiece_ >= 0) {
        changed = splitVideo();
        if (changed) selPiece_ = pr_.locate(t + 0.001, nullptr);
    } else {
        QList<int> aIds, iIds;
        for (const AudioClip& a : pr_.audios) aIds << a.id;
        for (const Item& it : pr_.items) iIds << it.id;
        changed = splitVideo();
        for (int id : aIds) changed |= splitAudio(id);
        for (int id : iIds) changed |= splitItem(id);
    }
    if (!changed) {
        undo_.pop_back();
        return;
    }
    modelEdited();
}

void MainWindow::doDelete() {
    if (selItem_ || selAudio_) return deleteSelected();
    if (pr_.pieces.size() < 2 || selPiece_ < 0 || selPiece_ >= pr_.pieces.size()) return;
    pushUndo();
    pr_.pieces.removeAt(selPiece_);
    selPiece_ = std::min<int>(selPiece_, pr_.pieces.size() - 1);
    modelEdited();
}

void MainWindow::doTrimStart() {
    if (pr_.pieces.isEmpty()) return;
    double src = 0;
    int i = pr_.locate(t_, &src);
    if (pr_.pieces[i].end - src >= kMinPiece) {
        pushUndo();
        pr_.pieces[i].start = src;
        selPiece_ = i;
        modelEdited();
    }
}

void MainWindow::doTrimEnd() {
    if (pr_.pieces.isEmpty()) return;
    double src = 0;
    int i = pr_.locate(t_, &src);
    if (src - pr_.pieces[i].start >= kMinPiece) {
        pushUndo();
        pr_.pieces[i].end = src;
        selPiece_ = i;
        modelEdited();
    }
}

void MainWindow::newItem(Item it) {
    double total = pr_.total();
    double t0 = t_, t1 = std::min(total, t0 + 3);
    if (t1 - t0 < 0.5) { t0 = std::max(0.0, t1 - 3); t1 = total; }
    it.t0 = t0;
    it.t1 = t1;
    it.id = pr_.nextId++;
    pushUndo();
    pr_.items.append(it);
    selItem_ = it.id;
    selPiece_ = -1;
    selAudio_ = 0;
    modelEdited();
}

void MainWindow::addBlur() {
    if (pr_.pieces.isEmpty()) return;
    Item it;
    it.kind = Item::Blur;
    it.x = 0.35; it.y = 0.35; it.w = 0.3; it.h = 0.2;
    newItem(it);
}

void MainWindow::addImage() {
    if (pr_.pieces.isEmpty()) return;
    QString f = QFileDialog::getOpenFileName(this, T("choose_image"), QString(),
                                             "Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)");
    if (f.isEmpty()) return;
    addToBin(f);
    addImageFile(f);
}

void MainWindow::addImageFile(const QString& f) {
    if (pr_.pieces.isEmpty()) return;
    QPixmap pm(f);
    if (pm.isNull()) return;
    Item it;
    it.kind = Item::Image;
    it.path = f;
    it.ar = double(pm.height()) / pm.width();
    it.x = 0.05; it.y = 0.05; it.w = 0.25;
    it.h = it.w * pr_.vw * it.ar / pr_.vh;
    newItem(it);
}

// ---------------------------------------------------------------- Zeichnen und Formen
void MainWindow::setTool(int tool) {
    if (tool && (pr_.pieces.isEmpty() || fullscreen_)) tool = 0;
    if (tool && !btnEdit_->isChecked()) btnEdit_->setChecked(true);  // Zeichnen gehört zum Bearbeiten
    const bool changed = tool != tool_;
    tool_ = tool;
    if (btnDraw_) btnDraw_->setChecked(tool == 1);
    if (btnShape_) btnShape_->setChecked(tool == 2);
    view_->setToolActive(tool != 0);
    if (tool == 1 && changed) drawItem_ = 0;  // jede Zeichen-Runde wird ein eigenes Element
    if (toolPreview_ && !tool) {
        scene_->removeItem(toolPreview_);
        delete toolPreview_;
        toolPreview_ = nullptr;
    }
    for (Overlay* ov : overlays_) ov->setAcceptedMouseButtons(showGuides() && !tool ? Qt::LeftButton : Qt::NoButton);
    if (tool && changed) showVideoHint(T(tool == 1 ? "draw_hint" : "shape_hint"), 2600);
}

static QPointF clampToCanvas(const QPointF& p, const Project& pr) {
    return QPointF(std::clamp(p.x(), 0.0, double(pr.vw)), std::clamp(p.y(), 0.0, double(pr.vh)));
}

void MainWindow::toolPress(const QPointF& sp) {
    const QPointF p = clampToCanvas(sp, pr_);
    toolPts_ = {p};
    if (!toolPreview_) {
        toolPreview_ = new QGraphicsPathItem;
        toolPreview_->setZValue(30);
        scene_->addItem(toolPreview_);
    }
    QPen pen(QColor::fromRgba(graphicColor_), std::max(1.0, 8.0 * pr_.vh / 1080.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    toolPreview_->setPen(pen);
    toolPreview_->setBrush(Qt::NoBrush);
    QPainterPath path;
    path.addEllipse(p, pen.widthF() / 2, pen.widthF() / 2);
    toolPreview_->setPath(path);
}

void MainWindow::toolMove(const QPointF& sp) {
    if (!toolPreview_ || toolPts_.isEmpty()) return;
    const QPointF p = clampToCanvas(sp, pr_);
    QPainterPath path;
    if (tool_ == 1) {
        if (QLineF(toolPts_.last(), p).length() < 1.5) return;
        toolPts_ << p;
        path.addPolygon(toolPts_);
    } else {
        if (toolPts_.size() < 2) toolPts_ << p;
        else toolPts_[1] = p;
        const QRectF r = QRectF(toolPts_[0], toolPts_[1]).normalized();
        if (shapeKind_ == Item::Rect) path.addRect(r);
        else if (shapeKind_ == Item::Ellipse) path.addEllipse(r);
        else { path.moveTo(toolPts_[0]); path.lineTo(toolPts_[1]); }
    }
    toolPreview_->setPath(path);
}

void MainWindow::toolRelease(const QPointF& sp) {
    toolMove(sp);
    if (toolPreview_) toolPreview_->setPath(QPainterPath());
    if (toolPts_.isEmpty() || pr_.pieces.isEmpty()) return;
    const double sw = std::max(1.0, 8.0 * pr_.vh / 1080.0);
    auto toBox = [this](const QRectF& r) {
        const QRectF c = r.intersected(QRectF(0, 0, pr_.vw, pr_.vh));
        return QRectF(c.x() / pr_.vw, c.y() / pr_.vh, std::max(c.width(), 4.0) / pr_.vw, std::max(c.height(), 4.0) / pr_.vh);
    };
    if (tool_ == 1) {
        // Alle Linien dieser Zeichnung in Bild-Koordinaten sammeln, Rahmen drumherum, dann auf den Rahmen beziehen
        QList<QPolygonF> lines;
        Item* old = drawItem_ ? pr_.item(drawItem_) : nullptr;
        if (old) {
            const QRectF ob(old->x * pr_.vw, old->y * pr_.vh, old->w * pr_.vw, old->h * pr_.vh);
            for (const QPolygonF& s : old->strokes) {
                QPolygonF q;
                for (const QPointF& pt : s) q << QPointF(ob.x() + pt.x() * ob.width(), ob.y() + pt.y() * ob.height());
                lines << q;
            }
        }
        lines << toolPts_;
        QRectF bb;
        for (const QPolygonF& s : lines) bb = bb.isNull() ? s.boundingRect() : bb.united(s.boundingRect());
        bb = bb.adjusted(-sw, -sw, sw, sw);
        const QRectF box = toBox(bb);
        const QRectF bpx(box.x() * pr_.vw, box.y() * pr_.vh, box.width() * pr_.vw, box.height() * pr_.vh);
        QList<QPolygonF> norm;
        for (const QPolygonF& s : lines) {
            QPolygonF q;
            for (const QPointF& pt : s) q << QPointF((pt.x() - bpx.x()) / bpx.width(), (pt.y() - bpx.y()) / bpx.height());
            norm << q;
        }
        if (old) {
            pushUndo();
            old->x = box.x(); old->y = box.y(); old->w = box.width(); old->h = box.height();
            old->strokes = norm;
            modelEdited(true);
            rebuildOverlays();
        } else {
            Item it;
            it.kind = Item::Draw;
            it.color = graphicColor_;
            it.x = box.x(); it.y = box.y(); it.w = box.width(); it.h = box.height();
            it.strokes = norm;
            newItem(it);
            drawItem_ = pr_.items.last().id;
        }
        for (Overlay* ov : overlays_) ov->setAcceptedMouseButtons(Qt::NoButton);  // weiterzeichnen statt verschieben
    } else {
        QPointF a = toolPts_[0], b = toolPts_.size() > 1 ? toolPts_[1] : toolPts_[0];
        if (QLineF(a, b).length() < 6) {  // nur geklickt: Form in Standardgröße um die Stelle
            const double w = pr_.vw * 0.25, h = shapeKind_ >= Item::Arrow ? pr_.vh * 0.12 : pr_.vh * 0.25;
            a = QPointF(a.x() - w / 2, a.y() - h / 2);
            b = QPointF(a.x() + w, a.y() + h);
            if (shapeKind_ >= Item::Arrow) { a.setY(a.y() + h / 2); b.setY(a.y()); }
        }
        Item it;
        it.kind = Item::Shape;
        it.shape = shapeKind_;
        it.color = graphicColor_;
        QRectF r = QRectF(a, b).normalized();
        if (shapeKind_ >= Item::Arrow) {
            // Platz für Strich und Pfeilspitze; Endpunkte beziehen sich auf den um sw/2 eingerückten Rahmen
            const double pad = sw / 2 + (shapeKind_ == Item::Arrow ? (sw * 4.5 + 10) * 0.5 : sw / 2);
            r = r.adjusted(-pad, -pad, pad, pad);
            const QRectF in = r.adjusted(sw / 2, sw / 2, -sw / 2, -sw / 2);
            auto rel = [&in](const QPointF& q) { return QPointF((q.x() - in.x()) / in.width(), (q.y() - in.y()) / in.height()); };
            it.strokes = {QPolygonF({rel(a), rel(b)})};
        }
        const QRectF box = toBox(r);
        it.x = box.x(); it.y = box.y(); it.w = box.width(); it.h = box.height();
        newItem(it);
        setTool(0);  // eine Form pro Klick auf "Form" - danach direkt verschieben/größer machen
    }
    toolPts_.clear();
}

// ---------------------------------------------------------------- Medien-Sammlung (links)

// Vorschau-Kachel 16:9 mit mittig eingepasstem Bild
static QIcon binIcon(const QImage& src, const QColor& bg, const QPixmap& symbol = QPixmap()) {
    QPixmap pm(240, 136);
    pm.fill(bg);
    QPainter p(&pm);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!src.isNull()) {
        const QImage s = src.scaled(pm.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage((pm.width() - s.width()) / 2, (pm.height() - s.height()) / 2, s);
    }
    if (!symbol.isNull()) p.drawPixmap((pm.width() - symbol.width()) / 2, (pm.height() - symbol.height()) / 2, symbol);
    return QIcon(pm);
}

QWidget* MainWindow::buildMediaPage() {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    auto* imp = mk("import", "add_media_tip", Ic::Open, "primary");
    connect(imp, &QPushButton::clicked, this, &MainWindow::importMedia);
    v->addWidget(imp);
    bin_ = new BinList;
    bin_->setDragEnabled(true);  // in die Timeline ziehen
    bin_->setDragDropMode(QAbstractItemView::DragOnly);
    bin_->setDefaultDropAction(Qt::CopyAction);
    bin_->setViewMode(QListView::IconMode);
    bin_->setIconSize(QSize(120, 68));
    bin_->setGridSize(QSize(128, 100));
    bin_->setResizeMode(QListView::Adjust);
    bin_->setMovement(QListView::Static);
    bin_->setWordWrap(false);
    bin_->setTextElideMode(Qt::ElideMiddle);
    bin_->setStyleSheet("QListWidget::item { padding: 2px; }");
    connect(bin_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* it) { insertFromBin(it->data(Qt::UserRole).toString()); });
    v->addWidget(bin_, 1);
    lblBinHint_ = new QLabel;
    lblBinHint_->setObjectName("hint");
    lblBinHint_->setWordWrap(true);
    v->addWidget(lblBinHint_);
    return w;
}

void MainWindow::importMedia() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, T("import"), QString(),
        T("media_files") + " (*.mp4 *.mov *.mkv *.m4v *.avi *.webm *.wmv *.flv *.ts "
                           "*.mp3 *.wav *.m4a *.aac *.flac *.ogg *.opus *.wma *.png *.jpg *.jpeg *.webp *.bmp *.gif);;*.*");
    if (files.isEmpty()) return;
    QStringList rest = files;
    // Noch nichts offen: das erste Video wird direkt geöffnet
    if (pr_.pieces.isEmpty())
        for (int i = 0; i < rest.size(); ++i)
            if (mediaKindOf(rest[i]) == MediaKind::Video) { openFile(rest.takeAt(i)); break; }
    for (const QString& f : rest) addToBin(f);
}

// Titel/Interpret/Album und eingebettetes Cover einer Tondatei: ffmpeg zeigt die Tags beim Einlesen an und schreibt
// das Cover (falls vorhanden) als Bild. Bis es fertig ist, steht der Dateiname da.
void MainWindow::showAudioInfo(const QString& path) {
    audioShown_ = path;
    AudioMeta& m = audioMeta_[path];
    if (m.title.isEmpty()) m.title = QFileInfo(path).completeBaseName();
    view_->setAudioInfo(m.title, m.sub, m.cover);
    if (m.done) return;
    m.done = true;
    if (!binDir_) binDir_ = std::make_unique<QTemporaryDir>();
    const QString jpg = binDir_->filePath(QString("cover%1.jpg").arg(audioMeta_.size()));
    auto* proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, path, jpg] {
        proc->deleteLater();
        AudioMeta& m = audioMeta_[path];
        QString title, artist, albumArtist, album;
        for (const QString& line : QString::fromUtf8(proc->readAllStandardError()).split('\n')) {
            if (line.contains("Duration:")) break;  // danach kommen nur noch die Spuren
            const int colon = line.indexOf(':');
            if (colon < 0) continue;
            const QString key = line.left(colon).trimmed().toLower(), val = line.mid(colon + 1).trimmed();
            if (val.isEmpty()) continue;
            if (key == "title" && title.isEmpty()) title = val;
            else if (key == "artist" && artist.isEmpty()) artist = val;
            else if (key == "album_artist" && albumArtist.isEmpty()) albumArtist = val;
            else if (key == "album" && album.isEmpty()) album = val;
        }
        if (artist.isEmpty()) artist = albumArtist;
        if (!title.isEmpty()) m.title = title;
        QStringList sub;
        if (!artist.isEmpty()) sub << artist;
        if (!album.isEmpty() && album != title) sub << album;
        m.sub = sub.join(QString::fromUtf8(" \u2022 "));
        const QImage cover(jpg);
        if (!cover.isNull()) m.cover = cover;
        if (audioShown_ == path) view_->setAudioInfo(m.title, m.sub, m.cover);
    });
    // "0:v:0?": nur wenn ein Bild eingebettet ist; ohne Bild bricht ffmpeg ab, die Tags stehen trotzdem in der Ausgabe
    proc->start(ffmpegPath(), {"-hide_banner", "-y", "-i", path, "-map", "0:v:0?", "-frames:v", "1", "-an",
                               "-vf", "scale='min(800,iw)':-2", jpg});
}

void MainWindow::addToBin(const QString& path) {
    for (int i = 0; i < bin_->count(); ++i)
        if (bin_->item(i)->data(Qt::UserRole).toString() == path) return;
    const MediaKind kind = mediaKindOf(path);
    const Theme& th = currentTheme();
    auto* it = new QListWidgetItem(QFileInfo(path).fileName());
    it->setData(Qt::UserRole, path);
    it->setToolTip(QDir::toNativeSeparators(path));
    if (kind == MediaKind::Image) {
        it->setIcon(binIcon(QImage(path), QColor(0, 0, 0)));
    } else if (kind == MediaKind::Audio) {
        it->setIcon(binIcon(QImage(), audioColor().darker(160), makeIcon(Ic::Music, Qt::white).pixmap(44, 44)));
    } else if (!editData_) {
        // Beim reinen Anschauen braucht niemand das Vorschaubild - erst beim Bearbeiten holen
        it->setIcon(binIcon(QImage(), QColor(0, 0, 0), makeIcon(Ic::AddVideo, th.muted).pixmap(40, 40)));
        pendingBinThumbs_ << path;
    } else {
        it->setIcon(binIcon(QImage(), QColor(0, 0, 0), makeIcon(Ic::AddVideo, th.muted).pixmap(40, 40)));
        // erstes Bild des Videos im Hintergrund holen
        if (!binDir_) binDir_ = std::make_unique<QTemporaryDir>();
        const QString jpg = binDir_->filePath(QString("bin%1.jpg").arg(bin_->count()));
        auto* p = new QProcess(this);
        connect(p, &QProcess::finished, this, [this, p, jpg, path] {
            p->deleteLater();
            const QImage im(jpg);
            if (im.isNull()) return;
            for (int i = 0; i < bin_->count(); ++i)
                if (bin_->item(i)->data(Qt::UserRole).toString() == path)
                    bin_->item(i)->setIcon(binIcon(im, QColor(0, 0, 0)));
        });
        p->start(ffmpegPath(), {"-hide_banner", "-v", "error", "-y", "-ss", "1", "-i", path, "-frames:v", "1",
                                "-vf", "scale=240:-2", jpg});
    }
    bin_->addItem(it);
    lblBinHint_->setText(T("bin_hint"));
}

// Doppelklick in der Sammlung: an der Abspielposition einfügen
void MainWindow::insertMediaAt(const QString& path, double t) {
    addToBin(path);  // aus dem Explorer gezogen: auch ins Medien-Panel
    if (!pr_.pieces.isEmpty()) seek(t);
    insertFromBin(path);
}

void MainWindow::insertFromBin(const QString& path) {
    if (pr_.pieces.isEmpty()) {
        if (mediaKindOf(path) == MediaKind::Video) openFile(path);
        return;
    }
    if (mediaKindOf(path) == MediaKind::Image) return addImageFile(path);
    MediaInfo mi = probeMedia(path);
    if (!mi.ok || mi.duration <= 0) {
        QMessageBox::warning(this, "Vidline", T("media_unreadable"));
        return;
    }
    if (mi.w > 0 && mi.h > 0) addVideoFile(path, mi);
    else addAudioFile(path, mi);
}

// ---------------------------------------------------------------- Text
void MainWindow::addText() {
    if (pr_.pieces.isEmpty()) return;
    ensureFonts();
    Item it;
    it.kind = Item::Text;
    it.text = T("text_ph");
    it.font = specialFonts().contains("Anton") ? QString("Anton") : defaultTextFont();
    it.w = 0.5;
    it.h = 0.16;
    it.x = 0.25;
    it.y = 0.42;
    btnEdit_->setChecked(true);
    newItem(it);
}

void MainWindow::pickItemColor(bool background) {
    Item* it = pr_.item(selItem_);
    if (it && it->graphic()) {
        const QColor c = QColorDialog::getColor(QColor::fromRgba(it->color), this, T("color"), QColorDialog::ShowAlphaChannel);
        if (!c.isValid()) return;
        pushUndo();
        it->color = c.rgba();
        graphicColor_ = it->color;  // nächste Zeichnung/Form in derselben Farbe
        for (Overlay* ov : overlays_) ov->update();
        refreshInspector();
        return;
    }
    if (!it || it->kind != Item::Text) return;
    const QColor cur = QColor::fromRgba(background ? it->bgColor : it->color);
    const QColor c = QColorDialog::getColor(cur, this, T("color"), QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;
    pushUndo();
    (background ? it->bgColor : it->color) = c.rgba();
    for (Overlay* ov : overlays_) ov->update();
    refreshInspector();
}

// ---------------------------------------------------------------- Bild anpassen
void MainWindow::editPieceTf(const std::function<void(Piece&)>& fn) {
    if (selPiece_ < 0 || selPiece_ >= pr_.pieces.size()) return;
    fn(pr_.pieces[selPiece_]);
    // Vorschau: zum bearbeiteten Abschnitt springen, damit man die Änderung sieht
    if (cur_ != selPiece_) seek(pr_.outStart(selPiece_) + 0.01);
    applyVideoTransform();
    timeline_->update();
}

// ---------------------------------------------------------------- Wellenformen
const WaveSet* MainWindow::waves(const QString& path) const {
    auto it = waves_.find(path);
    return it == waves_.end() ? nullptr : it->second.get();
}

// Ton der Datei grob abtasten (4 kHz mono) und 50 Spitzenwerte pro Sekunde behalten
void MainWindow::startWaves(const QString& path) {
    if (path.isEmpty() || waves_.count(path)) return;
    auto ws = std::make_shared<WaveSet>();
    ws->rate = 50;
    waves_[path] = ws;
    auto* p = new QProcess(this);
    auto carry = std::make_shared<QByteArray>();
    auto acc = std::make_shared<std::pair<int, float>>(0, 0.f);
    auto chunks = std::make_shared<int>(0);
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p, ws, carry, acc, chunks] {
        const QByteArray d = *carry + p->readAllStandardOutput();
        const int n = int(d.size() / 2);
        const auto* smp = reinterpret_cast<const qint16*>(d.constData());
        for (int i = 0; i < n; ++i) {
            const float v = std::abs(int(smp[i])) / 32768.f;
            if (v > acc->second) acc->second = v;
            if (++acc->first == 80) {
                ws->peaks.push_back(acc->second);
                acc->first = 0;
                acc->second = 0;
            }
        }
        *carry = d.mid(n * 2);
        if (++*chunks % 40 == 0) timeline_->update();
    });
    connect(p, &QProcess::finished, this, [this, p, ws, acc] {
        if (acc->first > 0) ws->peaks.push_back(acc->second);
        p->deleteLater();
        timeline_->update();
    });
    p->start(ffmpegPath(), {"-hide_banner", "-v", "error", "-i", path, "-vn", "-ac", "1", "-ar", "4000", "-f", "s16le", "-"});
}

void MainWindow::timelineZoomed() {
    // Abspielposition nach dem Zoomen in der Mitte halten
    QTimer::singleShot(0, this, [this] {
        tlScroll_->horizontalScrollBar()->setValue(int(timeline_->playheadX() - tlScroll_->viewport()->width() / 2));
    });
}

// Weiteres Video an der Playhead-Position einfügen
void MainWindow::addVideoFile(const QString& f, const MediaInfo& mi) {
    pushUndo();
    Source s;
    s.path = f; s.duration = mi.duration; s.fps = mi.fps; s.hasAudio = mi.hasAudio; s.probed = true;
    pr_.sources.push_back(s);
    const int si = pr_.sources.size() - 1;

    double src = 0;
    int i = pr_.locate(t_, &src);
    const Piece p = pr_.pieces[i];
    int at;
    if (src - p.start < kMinPiece) {
        at = i;
    } else if (p.end - src < kMinPiece) {
        at = i + 1;
    } else {
        pr_.pieces[i].end = src;
        Piece rest = p;
        rest.start = src;
        pr_.pieces.insert(i + 1, rest);
        at = i + 1;
    }
    pr_.pieces.insert(at, Piece{si, 0.0, mi.duration, 1.0, 1.0});
    selPiece_ = at;
    selItem_ = selAudio_ = 0;
    t_ = pr_.outStart(at);
    startThumbs(si);
    startWaves(f);
    modelEdited();
}

void MainWindow::addAudioFile(const QString& f, const MediaInfo& mi) {
    AudioClip a;
    a.id = pr_.nextId++;
    a.path = f;
    a.t0 = t_;
    a.fileDur = mi.duration;
    a.dur = std::min(mi.duration, std::max(0.5, pr_.total() - t_));
    pushUndo();
    pr_.audios.append(a);
    startWaves(f);
    selAudio_ = a.id;
    selPiece_ = -1;
    selItem_ = 0;
    modelEdited();
}

// ---------------------------------------------------------------- Export
void MainWindow::exportVideo() {
    if (pr_.pieces.isEmpty() || exportProc_) return;
    player_->pause();
    ensureProbed();
    ExportDialog opt(this, pr_.vw, pr_.vh, pr_.sources[0].fps, pr_.sources[0].path);
    opt.setEstimateInputs(pr_.total(), pr_.sources[0].duration, pr_.natW, pr_.natH,
                          pr_.sources[0].hasAudio || !pr_.audios.isEmpty());
    if (opt.exec() != QDialog::Accepted) return;
    ExportOptions eo = opt.options();
    const bool replace = opt.replaceOriginal();
    const QString original = pr_.sources[0].path;
    const QString finalPath = opt.outputPath();
    // Beim Ersetzen erst in eine Zwischendatei schreiben (ffmpeg liest das Original noch)
    const QString out = replace ? QFileInfo(finalPath).dir().filePath(".vidline-export-" +
                                  QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + eo.format)
                                : finalPath;

    // Texte in Export-Auflösung als Bilder rendern (sieht genauso aus wie in der Vorschau)
    Project ep = pr_;
    auto textDir = std::make_shared<QTemporaryDir>();
    for (Item& it : ep.items) {
        if (it.kind != Item::Text && !it.graphic()) continue;
        const QString png = textDir->filePath(QString("text%1.png").arg(it.id));
        renderItemImage(it, QSize(std::max(2, int(it.w * eo.width)), std::max(2, int(it.h * eo.height))), eo.height).save(png);
        it.path = png;
    }
    QStringList args = buildExport(ep, eo, out);
    const double total = pr_.total();
    auto* dlg = new QProgressDialog(T("exporting"), T("cancel"), 0, 1000, this);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setMinimumDuration(0);
    dlg->setAutoClose(false);
    dlg->setAutoReset(false);
    dlg->setValue(0);

    exportProc_ = new QProcess(this);
    auto* proc = exportProc_;
    auto errText = std::make_shared<QString>();
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, dlg, total] {
        while (proc->canReadLine()) {
            QString line = QString::fromUtf8(proc->readLine()).trimmed();
            if (line.startsWith("out_time_us=")) {
                double s = line.mid(12).toLongLong() / 1e6;
                dlg->setValue(int(std::min(1.0, s / std::max(total, 0.01)) * 1000));
            }
        }
    });
    connect(proc, &QProcess::readyReadStandardError, this,
            [proc, errText] { *errText += QString::fromUtf8(proc->readAllStandardError()); });
    connect(dlg, &QProgressDialog::canceled, proc, [proc] { proc->kill(); });
    connect(proc, &QProcess::finished, this, [this, proc, dlg, out, errText, replace, original, finalPath, textDir](int code, QProcess::ExitStatus st) {
        bool cancelled = st == QProcess::CrashExit;
        dlg->close();
        dlg->deleteLater();
        exportProc_ = nullptr;
        proc->deleteLater();
        if (cancelled || code != 0) {
            if (replace) QFile::remove(out);
            if (!cancelled) QMessageBox::critical(this, T("export_fail"), errText->right(1500));
            return;
        }
        QString saved = out;
        if (replace) {
            // Datei freigeben, Original ersetzen, Ergebnis öffnen
            player_->stop();
            player_->setSource(QUrl());
            for (auto& [id, ap] : audioPlayers_) ap.p->setSource(QUrl());
            QApplication::processEvents();
            thumbSets_.erase(original);
            thumbSets_.erase(finalPath);
            bool ok = true;
            if (QFileInfo::exists(finalPath) && !QFile::remove(finalPath)) ok = false;
            if (ok && original != finalPath) QFile::remove(original);
            if (ok) ok = QFile::rename(out, finalPath);
            if (!ok) {
                QMessageBox::critical(this, T("export_fail"), QDir::toNativeSeparators(out));
                return;
            }
            saved = finalPath;
            openFile(finalPath);
        }
        QMessageBox::information(this, T("done"), T("saved") + "\n" + QDir::toNativeSeparators(saved));
    });
    proc->start(ffmpegPath(), args);
}

// Entwickler-Test: Fenster per grab() in Dateien rendern (greift nicht auf den Bildschirm zu)
void MainWindow::selfShots(const QString& dir) {
    audio_->setMuted(true);  // Tests laufen lautlos
    if (qEnvironmentVariableIsSet("VIDLINE_LANG")) {  // Website-Bilder in einer festen Sprache
        setLanguage(qEnvironmentVariable("VIDLINE_LANG"));
        retranslate();
    }
    auto shot = [this, dir](const QString& name) { grab().save(dir + "/" + name + ".png"); };
    QTimer::singleShot(3500, this, [=] {
        shot("a_default");
        btnEdit_->setChecked(true);
        seek(2.0);
        doSplit();
        addBlur();
        seek(2.5);
        addText();
        selectPiece(0);
        sliderPieceVol_->setValue(50);
        // Spulen: viele Ziehschritte hintereinander dürfen den Player nicht fluten
        for (int k = 0; k < 40; ++k) scrub(pr_.total() * k / 40.0);
        seek(1.0);
        showScrubPreview(4200, slider_);
    });
    QTimer::singleShot(5500, this, [=] {
        shot("b_edit");
        scrubPrev_->hide();
        selectItem(pr_.items.last().id);
        seek(3.0);
    });
    QTimer::singleShot(6000, this, [=] {
        shot("b2_text");
    });
    QTimer::singleShot(6500, this, [=] {
        shot("b3_media");
        enterFullscreen();
    });
    QTimer::singleShot(7500, this, [=] {
        shot("c_fullscreen");
        exitFullscreen();
        setCurrentTheme("xp");
        applyTheme();
    });
    QTimer::singleShot(9500, this, [=] {
        shot("d_xp");
        setCurrentTheme("light");
        applyTheme();
        setLanguage("ja");
        retranslate();
    });
    QTimer::singleShot(11500, this, [=] {
        shot("e_light_ja");
        setCurrentTheme("studio");
        setLanguage("de");
        applyTheme();
        retranslate();
        SettingsDialog d(this);
        QTimer::singleShot(300, &d, [&d, dir] { d.grab().save(dir + "/f_settings.png"); d.accept(); });
        d.exec();
        ShortcutDialog sd(this);
        QTimer::singleShot(300, &sd, [&sd, dir] { sd.grab().save(dir + "/f2_shortcuts.png"); sd.reject(); });
        sd.exec();
        ExportDialog e(this, pr_.vw, pr_.vh, 30.0, pr_.sources.isEmpty() ? QString() : pr_.sources[0].path);
        if (!pr_.sources.isEmpty()) e.setEstimateInputs(pr_.total(), pr_.sources[0].duration, pr_.natW, pr_.natH, true);
        QTimer::singleShot(2500, &e, [&e, dir] { e.grab().save(dir + "/g_export.png"); e.accept(); });  // Probe-Export abwarten
        e.exec();
    });
    // Alle Themes einzeln (für die Website)
    QTimer::singleShot(13000, this, [=] {
        setLanguage(qEnvironmentVariable("VIDLINE_LANG", "en"));
        retranslate();
        const auto list = themes();
        for (int i = 0; i < list.size(); ++i) {
            QTimer::singleShot(i * 900, this, [=] {
                setCurrentTheme(list[i].id);
                applyTheme();
                QTimer::singleShot(400, this, [=] { shot("theme_" + list[i].id); });
            });
        }
        QTimer::singleShot(list.size() * 900 + 800, this, [=] {
            // Export-Test: Text + verschobener/gedrehter Abschnitt, als MP4, GIF und MP3
            if (!pr_.pieces.isEmpty()) {
                pr_.pieces.last().scale = 0.7;
                pr_.pieces.last().rot = 8;
                pr_.pieces.last().px = 0.1;
            }
            ensureProbed();
            for (const char* fmt : {"mp4", "gif", "mp3"}) {
                ExportOptions eo;
                eo.width = 640; eo.height = 360; eo.fps = 30; eo.format = fmt;
                Project ep = pr_;
                for (Item& it : ep.items)
                    if (it.kind == Item::Text) {
                        it.path = dir + QString("/text%1.png").arg(it.id);
                        renderTextImage(it, QSize(int(it.w * 640), int(it.h * 360))).save(it.path);
                    }
                QProcess pr;
                pr.start(ffmpegPath(), buildExport(ep, eo, dir + "/export." + fmt));
                pr.waitForFinished(180000);
                QFile log(dir + QString("/export_%1.txt").arg(fmt));
                log.open(QIODevice::WriteOnly);
                log.write(QString("exit=%1\n").arg(pr.exitCode()).toUtf8() + pr.readAllStandardError().right(3000));
            }
            QApplication::quit();
        });
    });
}

// Entwickler-Test (lautlos, unsichtbar): Leiste, Leertaste halten, Spulen, Kürzen, Verschieben, Zeichnen, Formen, Ton-Datei.
// Aufruf: Vidline <video> --fsshots <ordner> [<tondatei>]
void MainWindow::fsShots(const QString& dir) {
    silent_ = true;
    audio_->setMuted(true);
    const QStringList args = QApplication::arguments();
    const QString audioFile = args.size() > 4 ? args[4] : QString();
    auto logLine = [dir](const QString& line) {
        QFile f(dir + "/fs.txt");
        f.open(QIODevice::Append | QIODevice::Text);
        f.write((line + "\n").toUtf8());
    };
    auto step = [this, dir, logLine](const QString& name) {
        grab().save(dir + "/fs_" + name + ".png");
        logLine(QString("%1: bar=%2 playing=%3 scrubbing=%4 boost=%5 rate=%6 muted=%7 t=%8 pieces=%9 items=%10 edit=%11")
                    .arg(name).arg(fsBar_->isVisible()).arg(player_->playbackState() == QMediaPlayer::PlayingState)
                    .arg(scrubbing_).arg(boosted_).arg(player_->playbackRate()).arg(audio_->isMuted())
                    .arg(t_, 0, 'f', 2).arg(pr_.pieces.size()).arg(pr_.items.size()).arg(btnEdit_->isChecked()));
    };
    auto key = [this](QEvent::Type type) {
        QKeyEvent ev(type, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(this, &ev);
    };
    auto mouse = [](QWidget* w, QEvent::Type type, QPointF pos) {
        QMouseEvent ev(type, pos, w->mapToGlobal(pos), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                       type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(w, &ev);
    };
    auto at = [this](int ms, std::function<void()> fn) { QTimer::singleShot(ms, this, fn); };
    // Ziehen über 1 s: wie viele echte Bilder zeigt das Video dabei?
    auto frames = std::make_shared<int>(0);
    connect(vitem_->videoSink(), &QVideoSink::videoFrameChanged, this, [frames] { ++*frames; });

    // Leiste beim Anschauen: kommt mit der Maus, geht nach ein paar Sekunden - auch pausiert; Weiterspielen holt sie nicht
    at(2500, [=] { enterFullscreen(); togglePlay(); });
    at(2700, [=] { step("01_play_start"); });
    at(5700, [=] { step("02_play_idle"); emit view_->mouseActivity(); });
    at(5900, [=] { step("03_mouse"); });
    at(8900, [=] { step("04_idle_again"); togglePlay(); });
    at(9100, [=] { step("05_paused_shows"); });
    at(12100, [=] { step("06_paused_idle_hides"); togglePlay(); *frames = 0; });
    at(12300, [=] { step("07_unpause_stays_hidden"); });
    // Leertaste halten = 2×, kurz tippen = Pause
    at(12400, [=] { logLine(QString("frames while playing 12.3-12.4s: %1").arg(*frames)); key(QEvent::KeyPress); });
    at(12950, [=] { step("08_space_held"); key(QEvent::KeyRelease); });
    at(13100, [=] { step("09_space_released"); key(QEvent::KeyPress); key(QEvent::KeyRelease); });
    at(13250, [=] { step("10_space_tap_pauses"); togglePlay(); });
    // Spulen am Fortschritt: pausiert + stumm, danach spielt es weiter
    at(13600, [=] {
        fsSlider_->setSliderDown(true);
        emit fsSlider_->sliderPressed();
        *frames = 0;
        for (int k = 1; k <= 25; ++k)
            QTimer::singleShot(k * 40, this, [=] {
                const int v = 2000 + k * 200;
                fsSlider_->setValue(v);
                emit fsSlider_->sliderMoved(v);
            });
    });
    at(14700, [=] {
        logLine(QString("scrub 1s: frames shown=%1 (25 mouse steps)").arg(*frames));
        step("11_while_scrub");
        fsSlider_->setSliderDown(false);
        emit fsSlider_->sliderReleased();
    });
    at(15300, [=] { step("12_after_scrub_plays"); togglePlay(); });

    // Bearbeiten: Ränder kürzen (Rand folgt der Maus, danach rückt alles zusammen), Abschnitte verschieben
    at(15500, [=] { exitFullscreen(); btnEdit_->setChecked(true); });
    at(16500, [=] {
        step("13_edit");
        seek(pr_.total() / 2);
        selectPiece(-1);
        doSplit();  // zwei Abschnitte
        logLine(QString("split: pieces=%1 total=%2").arg(pr_.pieces.size()).arg(pr_.total(), 0, 'f', 2));
    });
    at(17000, [=] {
        // rechten Rand des ersten Abschnitts nach links ziehen
        const double x1 = 14 + pr_.outStart(1) * (timeline_->width() - 28) / pr_.total();
        const double y = 26 + 40;
        logLine(QString("trim press x=%1 tlw=%2").arg(x1, 0, 'f', 1).arg(timeline_->width()));
        mouse(timeline_, QEvent::MouseButtonPress, QPointF(x1 - 3, y));
        mouse(timeline_, QEvent::MouseMove, QPointF(x1 - 80, y));
        mouse(timeline_, QEvent::MouseMove, QPointF(x1 - 160, y));
        logLine(QString("while trim: total=%1 tlw=%2 scrubbing=%3").arg(pr_.total(), 0, 'f', 2).arg(timeline_->width()).arg(scrubbing_));
        timeline_->grab().save(dir + "/fs_14_trim_drag.png");
    });
    at(17400, [=] {
        const double x1 = 14 + pr_.outStart(1) * (timeline_->width() - 28) / pr_.total();
        mouse(timeline_, QEvent::MouseButtonRelease, QPointF(x1, 66));
        timeline_->grab().save(dir + "/fs_15_trim_released.png");
        logLine(QString("after trim: total=%1 piece0=%2..%3").arg(pr_.total(), 0, 'f', 2)
                    .arg(pr_.pieces[0].start, 0, 'f', 2).arg(pr_.pieces[0].end, 0, 'f', 2));
    });
    at(17800, [=] {
        // ersten Abschnitt hinter den zweiten ziehen
        const double w = timeline_->width() - 28, tot = pr_.total();
        const double xa = 14 + pr_.outStart(0) * w / tot + pr_.pieces[0].outDur() * w / tot / 2;
        const double xb = 14 + pr_.outStart(1) * w / tot + pr_.pieces[1].outDur() * w / tot * 0.9;
        logLine(QString("reorder before: first=%1 second=%2").arg(pr_.pieces[0].start, 0, 'f', 2).arg(pr_.pieces[1].start, 0, 'f', 2));
        mouse(timeline_, QEvent::MouseButtonPress, QPointF(xa, 50));
        mouse(timeline_, QEvent::MouseMove, QPointF(xa + 20, 50));
        mouse(timeline_, QEvent::MouseMove, QPointF(xb, 50));
        timeline_->grab().save(dir + "/fs_16_reorder_drag.png");
        mouse(timeline_, QEvent::MouseButtonRelease, QPointF(xb, 50));
        logLine(QString("reorder after: first=%1 second=%2").arg(pr_.pieces[0].start, 0, 'f', 2).arg(pr_.pieces[1].start, 0, 'f', 2));
    });
    // Zeichnen und Formen
    at(18200, [=] {
        seek(0.5);
        setTool(1);
        const double W = pr_.vw, H = pr_.vh;
        emit view_->toolPressed(QPointF(W * 0.2, H * 0.3));
        for (int k = 1; k <= 20; ++k) emit view_->toolMoved(QPointF(W * (0.2 + k * 0.01), H * (0.3 + std::sin(k / 3.0) * 0.08)));
        emit view_->toolReleased(QPointF(W * 0.41, H * 0.3));
        emit view_->toolPressed(QPointF(W * 0.25, H * 0.5));
        emit view_->toolMoved(QPointF(W * 0.35, H * 0.55));
        emit view_->toolReleased(QPointF(W * 0.35, H * 0.55));
        setTool(0);
        logLine(QString("draw: items=%1 kind=%2 strokes=%3").arg(pr_.items.size()).arg(pr_.items.last().kind).arg(pr_.items.last().strokes.size()));
        for (int k : {Item::Rect, Item::Ellipse, Item::Arrow}) {
            shapeKind_ = k;
            setTool(2);
            emit view_->toolPressed(QPointF(W * (0.55 + k * 0.1), H * 0.2));
            emit view_->toolMoved(QPointF(W * (0.62 + k * 0.1), H * 0.45));
            emit view_->toolReleased(QPointF(W * (0.62 + k * 0.1), H * 0.45));
        }
        logLine(QString("shapes: items=%1 tool=%2").arg(pr_.items.size()).arg(tool_));
    });
    at(18800, [=] {
        step("17_draw_shapes");
        // Export mit Zeichnung und Formen
        ensureProbed();
        ExportOptions eo;
        eo.width = 640; eo.height = 360; eo.fps = 30; eo.format = "mp4";
        Project ep = pr_;
        for (Item& it : ep.items)
            if (it.kind == Item::Text || it.graphic()) {
                it.path = dir + QString("/item%1.png").arg(it.id);
                renderItemImage(it, QSize(std::max(2, int(it.w * 640)), std::max(2, int(it.h * 360))), 360).save(it.path);
            }
        QProcess pr;
        pr.start(ffmpegPath(), buildExport(ep, eo, dir + "/export_draw.mp4"));
        pr.waitForFinished(180000);
        logLine(QString("export draw exit=%1 %2").arg(pr.exitCode()).arg(QString::fromUtf8(pr.readAllStandardError().right(400))));
        QProcess fr;  // ein Bild aus dem Export zum Ansehen
        fr.start(ffmpegPath(), {"-v", "error", "-y", "-ss", "0.6", "-i", dir + "/export_draw.mp4", "-frames:v", "1", dir + "/export_draw.png"});
        fr.waitForFinished(30000);
    });
    // Tondatei öffnen und als Video exportieren
    at(20000, [=] {
        if (audioFile.isEmpty()) return;
        btnEdit_->setChecked(false);
        openFile(audioFile);
    });
    at(22000, [=] {
        if (!audioFile.isEmpty()) {
            step("18_audio_file");
            togglePlay();
        }
    });
    at(23000, [=] {
        if (!audioFile.isEmpty()) {
            step("19_audio_playing");
            ensureProbed();
            logLine(QString("audio: video=%1 dur=%2 fps=%3").arg(pr_.sources[0].video).arg(pr_.sources[0].duration, 0, 'f', 2).arg(pr_.sources[0].fps));
            ExportOptions eo;
            eo.width = 640; eo.height = 360; eo.fps = 30; eo.format = "mp4";
            QProcess pr;
            pr.start(ffmpegPath(), buildExport(pr_, eo, dir + "/export_audio.mp4"));
            pr.waitForFinished(180000);
            logLine(QString("export audio exit=%1 %2").arg(pr.exitCode()).arg(QString::fromUtf8(pr.readAllStandardError().right(400))));
        }
        // eigene Akzentfarbe + Einstellungsfenster
        setAccentColor("#22c55e");
        applyTheme();
        step("20_accent_green");
        setCurrentTheme("minimal");
        setAccentColor(QString());
        btnEdit_->setChecked(true);
        applyTheme();
        step("20b_minimal");
        setCurrentTheme("minimal_light");
        applyTheme();
        step("20c_minimal_light");
        SettingsDialog d(this);
        QTimer::singleShot(300, &d, [&d, dir] { d.grab().save(dir + "/fs_21_settings.png"); d.accept(); });
        d.exec();
        setAccentColor(QString());
        QApplication::quit();
    });
}

void MainWindow::aspectShots(const QString& dir) {
    audio_->setMuted(true);  // Tests laufen lautlos
    auto shot = [this, dir](const QString& name) { grab().save(dir + "/" + name + ".png"); };
    auto log = [dir](const QString& line) {
        QFile f(dir + "/aspect.txt");
        if (f.open(QIODevice::Append)) f.write((line + QChar(10)).toUtf8());
    };
    QTimer::singleShot(3500, this, [=] {
        btnEdit_->setChecked(true);
        seek(1.0);
        addText();
        log(QString("start aspect=%1 canvas=%2x%3").arg(pr_.aspect).arg(pr_.vw).arg(pr_.vh));
        setAspect(2);  // 9:16
        log(QString("9:16 canvas=%1x%2 box=%3").arg(pr_.vw).arg(pr_.vh).arg(aspectBox_->currentText()));
    });
    QTimer::singleShot(4500, this, [=] {
        shot("aspect_9_16");
        ensureProbed();
        ExportDialog e(this, pr_.vw, pr_.vh, 30.0, pr_.sources[0].path);
        ExportOptions eo = e.options();
        eo.format = "mp4";
        log(QString("export %1x%2").arg(eo.width).arg(eo.height));
        Project ep = pr_;
        for (Piece& p : ep.pieces) p.denoise = true;  // Rauschunterdrückung im Export mittesten
        for (Item& it : ep.items)
            if (it.kind == Item::Text) {
                it.path = dir + QString("/text%1.png").arg(it.id);
                renderTextImage(it, QSize(int(it.w * eo.width), int(it.h * eo.height))).save(it.path);
            }
        QProcess pr;
        pr.start(ffmpegPath(), buildExport(ep, eo, dir + "/export_9_16.mp4"));
        pr.waitForFinished(180000);
        log(QString("export exit=%1 %2").arg(pr.exitCode()).arg(QString::fromUtf8(pr.readAllStandardError().left(3000))));
        setAspect(3);  // 1:1
        log(QString("1:1 canvas=%1x%2").arg(pr_.vw).arg(pr_.vh));
    });
    QTimer::singleShot(5500, this, [=] {
        shot("aspect_1_1");
        doUndo();
        log(QString("undo aspect=%1 canvas=%2x%3").arg(pr_.aspect).arg(pr_.vw).arg(pr_.vh));
    });
    QTimer::singleShot(6500, this, [=] {
        shot("aspect_undo");
        QApplication::quit();
    });
}
