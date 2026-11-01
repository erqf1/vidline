#include "dialogs.h"
#include "fileicons.h"
#include <QProcess>
#include <QColorDialog>
#include <cmath>
#include <QLocale>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include "i18n.h"
#include "shortcuts.h"
#include "icons.h"
#include "theme.h"

// ---------------------------------------------------------------- Sprache (Erststart)
LanguageDialog::LanguageDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Language / Sprache");
    setMinimumSize(360, 460);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    auto* title = new QLabel("Choose your language  ·  Sprache wählen");
    title->setObjectName("title");
    v->addWidget(title);
    list_ = new QListWidget;
    list_->addItems(languageNames());
    list_->setCurrentRow(1);
    v->addWidget(list_, 1);
    auto* ok = new QPushButton("OK");
    ok->setObjectName("primary");
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(list_, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    v->addWidget(ok);
}

QString LanguageDialog::code() const { return languageCodes().value(list_->currentRow(), "en"); }

// ---------------------------------------------------------------- Einstellungen
TagDialog::TagDialog(QWidget* parent, const QString& path, const AudioTags& tags)
    : QDialog(parent), tags_(tags), coverOk_(canEmbedCover(path)) {
    setWindowTitle(T("tags_title"));
    setAcceptDrops(coverOk_);
    resize(640, 400);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 18);
    root->setSpacing(16);
    auto* top = new QHBoxLayout;
    top->setSpacing(20);

    auto* left = new QVBoxLayout;
    left->setSpacing(8);
    cover_ = new QLabel;
    cover_->setFixedSize(220, 220);
    left->addWidget(cover_);
    auto* pick = new QPushButton(T("tags_pick"));
    remove_ = new QPushButton(T("tags_remove"));
    left->addWidget(pick);
    left->addWidget(remove_);
    if (!coverOk_) {
        pick->hide();
        remove_->hide();
        auto* no = new QLabel(T("tags_nocover"));
        no->setObjectName("hint");
        no->setWordWrap(true);
        left->addWidget(no);
    }
    left->addStretch();
    top->addLayout(left);

    auto* form = new QFormLayout;
    form->setSpacing(9);
    auto field = [&](const char* key, const QString& value) {
        auto* e = new QLineEdit(value);
        form->addRow(T(key), e);
        return e;
    };
    title_ = field("tag_title", tags.title);
    artist_ = field("tag_artist", tags.artist);
    album_ = field("tag_album", tags.album);
    albumArtist_ = field("tag_albumartist", tags.albumArtist);
    year_ = field("tag_year", tags.year);
    track_ = field("tag_track", tags.track);
    genre_ = field("tag_genre", tags.genre);
    top->addLayout(form, 1);
    root->addLayout(top, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new QPushButton(T("cancel"));
    auto* save = new QPushButton(T("tags_save"));
    save->setObjectName("primary");
    save->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(pick, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, T("tags_pick"), QString(), "*.png *.jpg *.jpeg *.bmp");
        if (f.isEmpty()) return;
        const QImage img(f);
        if (img.isNull()) return;
        tags_.coverChanged = true;
        setCover(img);
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        tags_.coverChanged = true;
        setCover(QImage());
    });
    setCover(tags.cover);
}

void TagDialog::setCover(const QImage& img) {
    tags_.cover = img;
    remove_->setEnabled(!img.isNull());
    const Theme& th = currentTheme();
    QPixmap pm(cover_->size());
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(pm.rect()), 12, 12);
    p.setClipPath(clip);
    if (img.isNull()) {
        p.fillRect(pm.rect(), th.panel2);
        p.drawPixmap(QRect(78, 78, 64, 64), makeIcon(Ic::Music, th.muted).pixmap(64, 64));
    } else {  // füllt das Quadrat, Überstehendes wird abgeschnitten
        const QImage s = img.scaled(pm.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawImage((pm.width() - s.width()) / 2, (pm.height() - s.height()) / 2, s);
    }
    p.end();
    cover_->setPixmap(pm);
}

AudioTags TagDialog::tags() const {
    AudioTags t = tags_;
    t.title = title_->text();
    t.artist = artist_->text();
    t.album = album_->text();
    t.albumArtist = albumArtist_->text();
    t.year = year_->text();
    t.track = track_->text();
    t.genre = genre_->text();
    return t;
}

void TagDialog::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasImage() || e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void TagDialog::dropEvent(QDropEvent* e) {
    QImage img;
    if (e->mimeData()->hasUrls() && !e->mimeData()->urls().isEmpty()) img = QImage(e->mimeData()->urls().first().toLocalFile());
    else if (e->mimeData()->hasImage()) img = qvariant_cast<QImage>(e->mimeData()->imageData());
    if (img.isNull()) return;
    tags_.coverChanged = true;
    setCover(img);
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setMinimumWidth(380);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    auto* form = new QFormLayout;
    form->setSpacing(12);
    lblLang_ = new QLabel;
    lblTheme_ = new QLabel;
    lang_ = new QComboBox;
    lang_->addItems(languageNames());
    lang_->setCurrentIndex(std::max<int>(0, languageCodes().indexOf(currentLanguage())));
    theme_ = new QComboBox;
    for (const Theme& t : themes()) theme_->addItem(t.name, t.id);
    theme_->setCurrentIndex(std::max<int>(0, theme_->findData(currentTheme().id)));
    form->addRow(lblLang_, lang_);
    form->addRow(lblTheme_, theme_);
    // Akzentfarbe: Vorgaben, eigene Farbe oder die des Designs
    lblAccent_ = new QLabel;
    auto* accentBox = new QWidget;
    auto* ag = new QGridLayout(accentBox);
    ag->setContentsMargins(0, 0, 0, 0);
    ag->setHorizontalSpacing(8);
    ag->setVerticalSpacing(8);
    const QList<QColor>& choices = accentChoices();
    for (int i = 0; i < choices.size(); ++i) {
        auto* b = new QPushButton;
        b->setFixedSize(28, 28);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("accent", choices[i].name());
        connect(b, &QPushButton::clicked, this, [this, c = choices[i].name()] {
            setAccentColor(c);
            emit accentChanged(c);
            markAccent();
        });
        swatches_ << b;
        ag->addWidget(b, 0, i);
    }
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(8);
    accentCustom_ = new QPushButton;
    accentDefault_ = new QPushButton;
    for (QPushButton* b : {accentCustom_, accentDefault_}) {
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        row2->addWidget(b);
    }
    row2->addStretch();
    ag->addLayout(row2, 1, 0, 1, int(choices.size()));
    connect(accentCustom_, &QPushButton::clicked, this, [this] {
        const QColor cur = currentTheme().accent;
        const QColor c = QColorDialog::getColor(cur, this, T("accent"));
        if (!c.isValid()) return;
        setAccentColor(c.name());
        emit accentChanged(c.name());
        markAccent();
    });
    connect(accentDefault_, &QPushButton::clicked, this, [this] {
        setAccentColor(QString());
        emit accentChanged(QString());
        markAccent();
    });
    form->addRow(lblAccent_, accentBox);
    // Dateisymbole im Explorer (nur installierte Windows-Version): wie Windows, hell oder dunkel
    if (fileIconsAvailable()) {
        lblIcons_ = new QLabel;
        icons_ = new QComboBox;
        for (const char* m : {"auto", "light", "dark"}) icons_->addItem(QString(), m);
        icons_->setCurrentIndex(std::max(0, icons_->findData(fileIconsMode())));
        connect(icons_, &QComboBox::currentIndexChanged, this, [this] { setFileIconsMode(icons_->currentData().toString()); });
        form->addRow(lblIcons_, icons_);
    }
    v->addLayout(form);
    markAccent();
    update_ = new QPushButton;
    connect(update_, &QPushButton::clicked, this, &SettingsDialog::checkUpdates);
    auto* ver = new QLabel("Vidline " APP_VERSION);
    ver->setObjectName("hint");
    auto* ur = new QHBoxLayout;
    ur->addWidget(ver);
    ur->addStretch();
    ur->addWidget(update_);
    v->addLayout(ur);
    keys_ = new QPushButton;
    connect(keys_, &QPushButton::clicked, this, [this] {
        ShortcutDialog dlg(this);
        dlg.exec();
    });
    v->addWidget(keys_);
    v->addStretch();
    ok_ = new QPushButton;
    ok_->setObjectName("primary");
    connect(ok_, &QPushButton::clicked, this, &QDialog::accept);
    v->addWidget(ok_);

    connect(lang_, &QComboBox::currentIndexChanged, this, [this](int i) {
        emit languageChanged(languageCodes().value(i, "en"));
        retranslate();
    });
    connect(theme_, &QComboBox::currentIndexChanged, this, [this] {
        emit themeChanged(theme_->currentData().toString());
        markAccent();
    });
    retranslate();
}

void SettingsDialog::markAccent() {
    const QString own = accentColor();
    const bool xp = currentTheme().xp;  // XP behält seine Farben
    const QColor ring = currentTheme().text;
    for (QPushButton* b : swatches_) {
        const QString c = b->property("accent").toString();
        const bool on = !own.isEmpty() && c.compare(own, Qt::CaseInsensitive) == 0;
        b->setStyleSheet(QString("QPushButton { background: %1; border-radius: 14px; padding: 0; min-width: 28px; max-width: 28px;"
                                 " min-height: 28px; max-height: 28px; border: %2; }")
                             .arg(c, on ? QString("3px solid %1").arg(ring.name()) : QString("2px solid transparent")));
        b->setEnabled(!xp);
    }
    accentCustom_->setEnabled(!xp);
    accentDefault_->setEnabled(!xp);
}

void SettingsDialog::retranslate() {
    setWindowTitle(T("settings"));
    lblLang_->setText(T("language"));
    lblTheme_->setText(T("theme"));
    lblAccent_->setText(T("accent"));
    accentCustom_->setText(T("custom_color"));
    accentDefault_->setText(T("theme_color"));
    if (icons_) {
        lblIcons_->setText(T("file_icons"));
        icons_->setItemText(0, T("fi_auto"));
        icons_->setItemText(1, T("fi_light"));
        icons_->setItemText(2, T("fi_dark"));
    }
    ok_->setText(T("ok"));
    update_->setText(T("upd_check"));
    keys_->setText(T("shortcuts") + "…");
}

// ---------------------------------------------------------------- Tastenkürzel
KeyButton::KeyButton(const QKeySequence& seq, QWidget* parent) : QPushButton(parent), seq_(seq) {
    setMinimumWidth(150);
    setCursor(Qt::PointingHandCursor);
    connect(this, &QPushButton::clicked, this, [this] { setRecording(!recording_); });
    setRecording(false);
}

void KeyButton::setSequence(const QKeySequence& seq) {
    seq_ = seq;
    setRecording(false);
}

void KeyButton::setRecording(bool on) {
    recording_ = on;
    if (on) {
        setText(T("sc_press"));
        setFocus();
        grabKeyboard();
    } else {
        releaseKeyboard();
        setText(seq_.isEmpty() ? QStringLiteral("—") : seq_.toString(QKeySequence::NativeText));
    }
}

void KeyButton::keyPressEvent(QKeyEvent* e) {
    if (!recording_) return QPushButton::keyPressEvent(e);
    const int key = e->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta ||
        key == Qt::Key_AltGr || key == Qt::Key_unknown)
        return;  // warten, bis eine "echte" Taste kommt
    const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (!mods && key == Qt::Key_Escape) return setRecording(false);  // abbrechen
    if (!mods && key == Qt::Key_Backspace) {
        seq_ = QKeySequence();
    } else {
        seq_ = QKeySequence(QKeyCombination(mods, Qt::Key(key)));
    }
    setRecording(false);
    emit sequenceChanged(seq_);
}

void KeyButton::focusOutEvent(QFocusEvent* e) {
    if (recording_) setRecording(false);
    QPushButton::focusOutEvent(e);
}

ShortcutDialog::ShortcutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(T("shortcuts"));
    setMinimumSize(480, 560);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(10);
    auto* hint = new QLabel(T("shortcuts_hint"));
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    v->addWidget(hint);

    auto* list = new QWidget;
    auto* grid = new QGridLayout(list);
    grid->setContentsMargins(0, 0, 8, 0);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(6);
    QList<KeyButton*> buttons;
    int row = 0;
    for (const ShortcutDef& d : shortcutDefs()) {
        auto* label = new QLabel(T(d.label));
        grid->addWidget(label, row, 0);
        auto* b = new KeyButton(shortcutFor(d.id));
        grid->addWidget(b, row, 1);
        buttons << b;
        ++row;
    }
    grid->setColumnStretch(0, 1);
    auto* scroll = new QScrollArea;
    scroll->setWidget(list);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    v->addWidget(scroll, 1);

    // Belegt eine Aktion eine Taste, die schon vergeben war, verliert die andere sie (mit Hinweis)
    auto* note = new QLabel;
    note->setObjectName("hint");
    note->setWordWrap(true);
    v->addWidget(note);
    const QList<ShortcutDef>& defs = shortcutDefs();
    for (int i = 0; i < buttons.size(); ++i) {
        connect(buttons[i], &KeyButton::sequenceChanged, this, [buttons, defs, i, note](const QKeySequence& seq) {
            note->clear();
            if (seq.isEmpty()) return;
            for (int k = 0; k < buttons.size(); ++k)
                if (k != i && buttons[k]->sequence() == seq) {
                    buttons[k]->setSequence(QKeySequence());
                    note->setText(T("sc_moved").arg(T(defs[k].label)));
                }
        });
    }

    auto* row2 = new QHBoxLayout;
    auto* reset = new QPushButton(T("sc_reset"));
    connect(reset, &QPushButton::clicked, this, [buttons, defs, note] {
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setSequence(defaultShortcut(defs[i].id));
        note->clear();
    });
    row2->addWidget(reset);
    row2->addStretch();
    auto* cancel = new QPushButton(T("cancel"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    row2->addWidget(cancel);
    auto* ok = new QPushButton(T("ok"));
    ok->setObjectName("primary");
    connect(ok, &QPushButton::clicked, this, [this, buttons, defs] {
        for (int i = 0; i < buttons.size(); ++i) setShortcut(defs[i].id, buttons[i]->sequence());
        accept();
    });
    row2->addWidget(ok);
    v->addLayout(row2);
}

// ---------------------------------------------------------------- Export
ExportDialog::ExportDialog(QWidget* parent, int srcW, int srcH, double srcFps, const QString& sourcePath)
    : QDialog(parent), srcW_(srcW), srcH_(srcH), srcFps_(srcFps > 1 ? srcFps : 30.0), source_(sourcePath) {
    setWindowTitle(T("export"));
    setMinimumWidth(420);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    auto* form = new QFormLayout;
    form->setSpacing(12);

    // Format: Video, GIF oder nur Ton
    format_ = new QComboBox;
    format_->addItem("MP4 (H.264)", "mp4");
    format_->addItem("MOV", "mov");
    format_->addItem("MKV", "mkv");
    format_->addItem("GIF", "gif");
    format_->addItem("MP3 – " + T("audio_only"), "mp3");
    format_->addItem("WAV – " + T("audio_only"), "wav");
    format_->addItem("M4A (AAC) – " + T("audio_only"), "m4a");
    form->addRow(T("format"), format_);

    res_ = new QComboBox;
    const int shortSide = std::min(srcW, srcH);
    res_->addItem(QString("%1 (%2×%3)").arg(T("original")).arg(srcW).arg(srcH), shortSide);
    struct R { int h; const char* name; };
    for (R r : {R{2160, "4K (2160p)"}, R{1440, "2K (1440p)"}, R{1080, "Full HD (1080p)"}, R{720, "HD (720p)"},
                R{480, "SD (480p)"}, R{360, "SD (360p)"}, R{240, "240p"}})
        if (r.h < shortSide) res_->addItem(r.name, r.h);
    form->addRow(T("resolution"), res_);

    fps_ = new QComboBox;
    const double f0 = srcFps > 1 ? srcFps : 30.0;
    fps_->addItem(QString("%1 (%2 fps)").arg(T("original")).arg(f0, 0, 'g', 5), f0);
    for (double f : {60.0, 50.0, 30.0, 25.0, 24.0, 15.0})
        if (f < f0 - 0.5) fps_->addItem(QString("%1 fps").arg(f, 0, 'g', 3), f);
    form->addRow(T("framerate"), fps_);

    quality_ = new QComboBox;
    quality_->addItem(T("q_high"), 0);
    quality_->addItem(T("q_mid"), 1);
    quality_->addItem(T("q_small"), 2);
    quality_->setCurrentIndex(1);
    form->addRow(T("quality"), quality_);
    v->addLayout(form);
    est_ = new QLabel;
    est_->setObjectName("title");
    v->addWidget(est_);
    for (QComboBox* c : {format_, res_, fps_, quality_})
        connect(c, &QComboBox::currentIndexChanged, this, &ExportDialog::updateEstimate);

    auto* hint = new QLabel(T("export_hint"));
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    v->addWidget(hint);

    // Ziel: Name + Speicherort (mit Durchsuchen) oder Originaldatei ersetzen
    v->addSpacing(6);
    auto* dest = new QLabel(T("save_to"));
    dest->setObjectName("title");
    v->addWidget(dest);
    auto* form2 = new QFormLayout;
    form2->setSpacing(10);
    const QFileInfo src(source_);
    name_ = new QLineEdit(src.completeBaseName() + "_edit");
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(name_, 1);
    ext_ = new QLabel(".mp4");
    nameRow->addWidget(ext_);
    form2->addRow(T("file_name"), nameRow);
    folder_ = new QLineEdit(QDir::toNativeSeparators(src.absolutePath()));
    browse_ = new QPushButton(T("browse"));
    connect(browse_, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("folder"), folder_->text());
        if (!d.isEmpty()) folder_->setText(QDir::toNativeSeparators(d));
    });
    auto* folderRow = new QHBoxLayout;
    folderRow->addWidget(folder_, 1);
    folderRow->addWidget(browse_);
    form2->addRow(T("folder"), folderRow);
    v->addLayout(form2);
    replace_ = new QCheckBox(T("replace_orig"));
    replace_->setToolTip(T("replace_hint"));
    auto* rhint = new QLabel(T("replace_hint"));
    rhint->setObjectName("hint");
    rhint->setWordWrap(true);
    rhint->hide();
    v->addWidget(replace_);
    v->addWidget(rhint);
    const QString savedName = name_->text();
    connect(replace_, &QCheckBox::toggled, this, [this, rhint, savedName](bool on) {
        const QFileInfo fi(source_);
        name_->setText(on ? fi.completeBaseName() : savedName);
        folder_->setText(QDir::toNativeSeparators(fi.absolutePath()));
        name_->setEnabled(!on);
        folder_->setEnabled(!on);
        browse_->setEnabled(!on);
        rhint->setVisible(on);
    });

    // Bei "nur Ton" gibt es keine Bild-Einstellungen; Original ersetzen nur bei Video-Formaten
    connect(format_, &QComboBox::currentIndexChanged, this, [this, form] {
        const QString f = format_->currentData().toString();
        const bool audio = isAudioFormat(f);
        form->setRowVisible(res_, !audio);
        form->setRowVisible(fps_, !audio && f != "gif");
        form->setRowVisible(quality_, f != "gif" && f != "wav");
        ext_->setText("." + f);
        const bool video = f == "mp4" || f == "mov" || f == "mkv";
        if (!video) replace_->setChecked(false);
        replace_->setEnabled(video);
        adjustSize();
    });

    auto* row = new QHBoxLayout;
    auto* cancel = new QPushButton(T("cancel"));
    auto* ok = new QPushButton(T("export"));
    ok->setObjectName("primary");
    ok->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(ok);
    v->addLayout(row);
}

void ExportDialog::setEstimateInputs(double outDuration, double srcDuration, int natW, int natH, bool hasAudio) {
    outDur_ = outDuration;
    srcDur_ = srcDuration;
    hasAudio_ = hasAudio;
    // Bitrate des Originals (ohne Ton) als Maß dafür, wie viel Bewegung/Detail im Video steckt
    const qint64 bytes = QFileInfo(source_).size();
    if (srcDuration > 0.5 && bytes > 0) srcVideoBps_ = std::max(0.0, bytes * 8.0 / srcDuration - (hasAudio ? 160000.0 : 0.0));
    srcPxRate_ = double(std::max(1, natW)) * std::max(1, natH) * srcFps_;
    updateEstimate();
}

void ExportDialog::showSize(double bits) {
    const double mb = bits / 8 / 1048576;
    const QString size = mb >= 1024 ? QString("%1 GB").arg(QLocale().toString(mb / 1024, 'f', 1))
                                    : QString("%1 MB").arg(mb < 10 ? QLocale().toString(mb, 'f', 1) : QString::number(qRound(mb)));
    est_->setText(T("est_size").arg(size));
}

// Ungefähre Dateigröße. Exportiert wird mit konstanter Qualität (CRF/CQP), die Größe hängt also vom Inhalt ab.
// Video: erst eine schnelle Schätzung aus der Bitrate des Originals, dann im Hintergrund 3 s aus der Mitte mit genau
// diesen Einstellungen kodieren und hochrechnen. Nur-Ton-Formate sind direkt fast exakt, GIF nur grob.
void ExportDialog::updateEstimate() {
    if (!est_ || outDur_ <= 0) return;
    if (probe_) {
        probe_->disconnect(this);
        probe_->kill();
        probe_->deleteLater();
        probe_ = nullptr;
    }
    const ExportOptions o = options();
    const int q = std::clamp(o.quality, 0, 2);
    double bits = 0;
    if (o.format == "wav") {
        bits = outDur_ * 48000 * 2 * 16;
    } else if (isAudioFormat(o.format)) {
        static const double kbps[3] = {320, 192, 128};
        bits = outDur_ * kbps[q] * 1000;
    } else if (o.format == "gif") {
        const double gw = std::min(o.width, 640), gh = double(o.height) * gw / std::max(1, o.width);
        bits = outDur_ * gw * gh * std::min(o.fps, 15.0) * 0.5;  // sehr grob: GIF schwankt stark mit dem Inhalt
    } else {
        static const double factor[3] = {1.0, 0.6, 0.38}, bppMin[3] = {0.03, 0.018, 0.011}, bppMax[3] = {0.25, 0.12, 0.07};
        const double pxRate = double(o.width) * o.height * o.fps;
        double vbps = srcVideoBps_ > 0 && srcPxRate_ > 0 ? srcVideoBps_ * std::pow(pxRate / srcPxRate_, 0.75) * factor[q]
                                                          : pxRate * 0.07 * factor[q];
        vbps = std::clamp(vbps, pxRate * bppMin[q], pxRate * bppMax[q]);
        bits = outDur_ * (vbps + (hasAudio_ ? 192000.0 : 0.0)) * 1.01;

        // Probe-Export: 3 s aus der Mitte des Originals, gleiche Größe/Bildrate/Qualität/Encoder
        if (!source_.isEmpty() && srcDur_ > 0.5) {
            const double len = std::min(3.0, srcDur_), from = std::max(0.0, srcDur_ / 2 - len / 2);
            const QString tmp = QDir::temp().filePath("vidline-size-probe.mp4");
            QStringList a = {"-hide_banner", "-v", "error", "-y", "-ss", QString::number(from, 'f', 2), "-t",
                             QString::number(len, 'f', 2), "-i", source_, "-an", "-vf",
                             QString("scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2,format=yuv420p")
                                 .arg(o.width).arg(o.height),
                             "-r", QString::number(o.fps, 'f', 3)};
            a << encArgs(o.encoder, q) << tmp;
            probe_ = new QProcess(this);
            const double audioBps = hasAudio_ ? 192000.0 : 0.0, dur = outDur_;
            connect(probe_, &QProcess::finished, this, [this, tmp, len, audioBps, dur](int code, QProcess::ExitStatus st) {
                const qint64 bytes = QFileInfo(tmp).size();
                QFile::remove(tmp);
                if (probe_) probe_->deleteLater();
                probe_ = nullptr;
                if (st != QProcess::NormalExit || code != 0 || bytes <= 0) return;  // Schätzung von oben bleibt stehen
                showSize(dur * (bytes * 8.0 / len + audioBps) * 1.01);
            });
            probe_->start(ffmpegPath(), a);
        }
    }
    showSize(bits);
}

ExportOptions ExportDialog::options() const {
    ExportOptions o;
    const double k = res_->currentData().toDouble() / std::max(1, std::min(srcW_, srcH_));
    o.width = std::max(2, int(std::lround(srcW_ * k / 2.0)) * 2);
    o.height = std::max(2, int(std::lround(srcH_ * k / 2.0)) * 2);
    o.fps = fps_->currentData().toDouble();
    o.quality = quality_->currentData().toInt();
    o.format = format_->currentData().toString();
    o.encoder = (o.format == "gif" || isAudioFormat(o.format)) ? QString("libx264") : detectEncoder();
    return o;
}

QString ExportDialog::outputPath() const {
    QString n = name_->text().trimmed();
    if (n.endsWith(ext(), Qt::CaseInsensitive)) n.chop(ext().size());
    return QDir(QDir::fromNativeSeparators(folder_->text().trimmed())).filePath(n + ext());
}

QString ExportDialog::ext() const { return "." + format_->currentData().toString(); }

bool ExportDialog::replaceOriginal() const { return replace_->isChecked(); }

void ExportDialog::accept() {
    if (name_->text().trimmed().isEmpty()) { name_->setFocus(); return; }
    if (!QDir(QDir::fromNativeSeparators(folder_->text().trimmed())).exists()) { folder_->setFocus(); return; }
    const QString out = outputPath();
    if (!replaceOriginal() && QFileInfo::exists(out) &&
        QMessageBox::question(this, T("export"), T("overwrite_q").arg(QFileInfo(out).fileName())) != QMessageBox::Yes)
        return;
    QDialog::accept();
}
