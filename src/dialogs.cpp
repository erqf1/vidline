#include "dialogs.h"
#include "defaultapps.h"
#include "fileicons.h"
#include <QProcess>
#include <QColorDialog>
#include <cmath>
#include <numeric>
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
#include <QInputDialog>
#include <QSettings>
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
    plugins_ = new QPushButton;
    connect(plugins_, &QPushButton::clicked, this, &SettingsDialog::openPlugins);
    v->addWidget(plugins_);
    // Als Standard-App festlegen (Videos und Musik)
    defaults_ = new QPushButton;
    connect(defaults_, &QPushButton::clicked, this, [this] {
        DefaultAppsDialog dlg(this);
        dlg.exec();
    });
    v->addWidget(defaults_);
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
    plugins_->setText(T("plugins") + "…");
    defaults_->setText(T("da_menu"));
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
    form_ = form;
    form->setSpacing(12);

    // Vorlage: TikTok, YouTube, Discord … oder eigene Einstellungen
    preset_ = new QComboBox;
    savePreset_ = new QPushButton(T("pr_save"));
    savePreset_->setToolTip(T("pr_save_tip"));
    delPreset_ = new QPushButton;
    delPreset_->setIcon(makeIcon(Ic::Trash, currentTheme().muted));
    delPreset_->setToolTip(T("pr_delete"));
    delPreset_->setFixedWidth(36);
    auto* presetRow = new QHBoxLayout;
    presetRow->addWidget(preset_, 1);
    presetRow->addWidget(savePreset_);
    presetRow->addWidget(delPreset_);
    form->addRow(T("pr_template"), presetRow);
    fit_ = new QComboBox;
    fit_->addItem(T("fit_pad"), 0);
    fit_->addItem(T("fit_crop"), 1);
    fit_->addItem(T("fit_blur"), 2);
    form->addRow(T("fit"), fit_);

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
    subs_ = new QComboBox;
    subs_->addItem(T("sub_exp_none"), 0);
    subs_->addItem(T("sub_exp_burn"), 1);
    subs_->addItem(T("sub_exp_track"), 2);
    subs_->setCurrentIndex(1);
    sidecar_ = new QCheckBox(T("sub_exp_srt"));
    auto* subRow = new QVBoxLayout;
    subRow->setSpacing(6);
    subRow->addWidget(subs_);
    subRow->addWidget(sidecar_);
    form->addRow(T("subtitles"), subRow);
    form->setRowVisible(subRow, false);
    v->addLayout(form);
    // Vorschau: Zielgröße als Rahmen, daneben Größe in Pixeln und ungefähre Dateigröße
    auto* pv = new QHBoxLayout;
    pv->setSpacing(14);
    preview_ = new FramePreview;
    pv->addWidget(preview_);
    auto* pvText = new QVBoxLayout;
    pvText->setSpacing(4);
    target_ = new QLabel;
    target_->setObjectName("hint");
    target_->setWordWrap(true);
    est_ = new QLabel;
    est_->setObjectName("title");
    est_->setWordWrap(true);
    pvText->addStretch();
    pvText->addWidget(est_);
    pvText->addWidget(target_);
    pvText->addStretch();
    pv->addLayout(pvText, 1);
    v->addLayout(pv);
    for (QComboBox* c : {format_, res_, fps_, quality_, fit_})
        connect(c, &QComboBox::currentIndexChanged, this, &ExportDialog::updateEstimate);
    connect(preset_, &QComboBox::currentIndexChanged, this, &ExportDialog::applyPreset);
    connect(savePreset_, &QPushButton::clicked, this, &ExportDialog::savePreset);
    connect(delPreset_, &QPushButton::clicked, this, [this] {
        const ExportPreset* p = currentPreset();
        if (!p || p->builtin) return;
        if (QMessageBox::question(this, T("pr_template"), T("pr_delete_q").arg(p->name)) != QMessageBox::Yes) return;
        removeCustomPreset(p->name);
        fillPresets(QString());
    });

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
        form->setRowVisible(res_, !audio && !currentPreset());
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
    fillPresets(QSettings().value("export/preset").toString());
}

void FramePreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Theme& th = currentTheme();
    const QRectF area = QRectF(rect()).adjusted(6, 6, -6, -6);
    auto fitIn = [](QSizeF s, QRectF box) {
        const double k = std::min(box.width() / s.width(), box.height() / s.height());
        const QSizeF r(s.width() * k, s.height() * k);
        return QRectF(box.center() - QPointF(r.width() / 2, r.height() / 2), r);
    };
    const QRectF frame = fitIn(QSizeF(std::max(1, target_.width()), std::max(1, target_.height())), area);
    const QSizeF cs(std::max(1, canvas_.width()), std::max(1, canvas_.height()));
    const double kPad = std::min(frame.width() / cs.width(), frame.height() / cs.height());
    const double kCrop = std::max(frame.width() / cs.width(), frame.height() / cs.height());
    const double k = fit_ == 1 ? kCrop : kPad;
    const QRectF pic(frame.center() - QPointF(cs.width() * k / 2, cs.height() * k / 2), cs * k);
    p.setPen(Qt::NoPen);
    if (fit_ == 2) {  // unscharfer Rand: verwaschene Fläche statt Schwarz
        QLinearGradient g(frame.topLeft(), frame.bottomRight());
        g.setColorAt(0, th.accent.darker(260));
        g.setColorAt(1, th.accent.darker(190));
        p.setBrush(g);
    } else {
        p.setBrush(QColor(0, 0, 0));
    }
    p.drawRoundedRect(frame, 4, 4);
    p.save();
    p.setClipRect(frame);
    QLinearGradient g(pic.topLeft(), pic.bottomLeft());
    g.setColorAt(0, th.accent.lighter(115));
    g.setColorAt(1, th.accent.darker(140));
    p.setBrush(g);
    p.drawRect(pic);
    p.restore();
    if (fit_ == 1 && (pic.width() > frame.width() + 1 || pic.height() > frame.height() + 1)) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(th.muted, 1, Qt::DashLine));
        p.drawRect(pic.intersected(QRectF(rect()).adjusted(1, 1, -1, -1)));
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(th.text, 1.5));
    p.drawRoundedRect(frame, 4, 4);
}

const ExportPreset* ExportDialog::currentPreset() const {
    return findPreset(presets_, preset_->currentData().toString());
}

QString ExportDialog::presetId() const { return preset_->currentData().toString(); }

void ExportDialog::fillPresets(const QString& select) {
    presets_ = allPresets();
    QSignalBlocker b(preset_);
    preset_->clear();
    preset_->addItem(T("pr_manual"), QString());
    for (const ExportPreset& p : presets_)
        preset_->addItem(p.builtin ? p.title() : p.title() + "  ·  " + T("pr_own"), p.id);
    preset_->setCurrentIndex(std::max(0, preset_->findData(select)));
    applyPreset();
}

void ExportDialog::selectPreset(const QString& id) { preset_->setCurrentIndex(std::max(0, preset_->findData(id))); }

void ExportDialog::setFit(int fit) { fit_->setCurrentIndex(std::max(0, fit_->findData(fit))); }

// Vorlage gewählt: Format und Rand übernehmen; Auflösung, Bildrate und Qualität legt die Vorlage fest
void ExportDialog::applyPreset() {
    const ExportPreset* p = currentPreset();
    QSettings().setValue("export/preset", presetId());
    delPreset_->setVisible(p && !p->builtin);
    for (QComboBox* c : {res_, fps_, quality_}) c->setEnabled(!p);
    format_->setEnabled(!p);
    if (p) {
        QSignalBlocker b1(fit_), b2(format_);
        fit_->setCurrentIndex(std::max(0, fit_->findData(p->fit)));
        format_->setCurrentIndex(std::max(0, format_->findData(p->format)));
        if (p->maxMB <= 0 && p->format != "gif") quality_->setCurrentIndex(std::max(0, quality_->findData(p->quality)));
        ext_->setText("." + p->format);
        const bool video = p->format == "mp4" || p->format == "mov" || p->format == "mkv";
        if (!video) replace_->setChecked(false);
        replace_->setEnabled(video);
    }
    const QString f = format_->currentData().toString();
    form_->setRowVisible(res_, !p && !isAudioFormat(f));
    updateEstimate();
    adjustSize();
}

void ExportDialog::savePreset() {
    bool ok = false;
    const ExportPreset* cur = currentPreset();
    const QString name = QInputDialog::getText(this, T("pr_save"), T("pr_name"), QLineEdit::Normal,
                                               cur && !cur->builtin ? cur->name : QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    ExportPreset p;
    if (cur) p = *cur;
    else {
        const ExportOptions o = options();
        p.w = o.finalW();
        p.h = o.finalH();
        p.maxFps = o.fps;
        p.format = o.format;
        p.quality = o.quality;
    }
    p.builtin = false;
    p.key = nullptr;
    p.name = name;
    p.id = "custom:" + name;
    p.fit = fit_->currentData().toInt();
    saveCustomPreset(p);
    fillPresets(p.id);
}

void ExportDialog::setHasSubtitles(bool on) {
    for (int r = 0; r < form_->rowCount(); ++r) {
        QLayoutItem* it = form_->itemAt(r, QFormLayout::FieldRole);
        if (it && it->layout() && it->layout()->indexOf(subs_) >= 0) form_->setRowVisible(r, on);
    }
    adjustSize();
}

int ExportDialog::subMode() const {
    if (!subs_->isVisibleTo(this)) return 0;
    const int m = subs_->currentData().toInt();
    const QString f = options().format;
    if (m == 2 && !(f == "mp4" || f == "mov" || f == "mkv")) return 0;
    return m;
}

bool ExportDialog::subSidecar() const { return subs_->isVisibleTo(this) && sidecar_->isChecked(); }

bool ExportDialog::twoPass() const {
    const ExportPreset* p = currentPreset();
    return p && p->maxMB > 0 && options().videoKbps > 0;
}

double ExportDialog::maxBytes() const {
    const ExportPreset* p = currentPreset();
    return p && !isAudioFormat(p->format) && p->format != "gif" ? p->maxMB * 1e6 : 0;
}

QString ExportDialog::estimateText() const { return est_->text() + "\n" + target_->text(); }

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

static QString fmtMB(double bytes) {
    const double mb = bytes / 1e6;
    return mb >= 1000 ? QString("%1 GB").arg(QLocale().toString(mb / 1000, 'f', 1))
                      : QString("%1 MB").arg(mb < 10 ? QLocale().toString(mb, 'f', 1) : QString::number(qRound(mb)));
}

void ExportDialog::showSize(double bits, const QString& extra) {
    est_->setText(T("est_size").arg(fmtMB(bits / 8)) + extra);
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
    const int fw = o.finalW(), fh = o.finalH();
    if (preview_) {
        preview_->set(QSize(srcW_, srcH_), QSize(fw, fh), o.fit);
        int g = std::gcd(fw, fh);
        QString t = isAudioFormat(o.format) ? T("audio_only") : QString("%1 × %2 · %3:%4").arg(fw).arg(fh).arg(fw / g).arg(fh / g);
        if (o.format == "gif" && !isAudioFormat(o.format)) {
            const int gw = std::max(2, int(std::lround(std::min(fw, o.gifWidth) / 2.0)) * 2);
            t = QString("%1 × %2 · GIF").arg(gw).arg(std::max(2, int(std::lround(double(fh) * gw / fw / 2.0)) * 2));
        }
        if (!isAudioFormat(o.format) && o.format != "gif") t += QString(" · %1 fps").arg(o.fps, 0, 'g', 4);
        if (!isAudioFormat(o.format) && std::min(fw, fh) > std::min(srcW_, srcH_) + 2) t += "\n" + T("pr_upscaled");
        if (o.outW > 0) t += "\n" + fit_->currentText();
        target_->setText(t);
        form_->setRowVisible(fit_, o.outW > 0 && !isAudioFormat(o.format));
    }
    if (o.videoKbps > 0) {
        // Ziel-Dateigröße: feste Bitrate in zwei Durchgängen, die Größe ist fast genau vorher bekannt
        const double bytes = outDur_ * (o.videoKbps + (hasAudio_ ? o.audioKbps : 0)) * 1000.0 / 8.0 * 1.01;
        const ExportPreset* p = currentPreset();
        QString extra = "\n" + T("pr_size_target").arg(fmtMB(p ? p->maxMB * 1e6 : bytes)).arg(o.videoKbps);
        if (p && outDur_ > 0 && sizeTargetVideoKbps(p->maxMB * 1e6, outDur_, hasAudio_ ? o.audioKbps : 0) < 100)
            extra += "\n" + T("pr_too_long");
        showSize(bytes * 8, extra);
        return;
    }
    if (o.format == "wav") {
        bits = outDur_ * 48000 * 2 * 16;
    } else if (isAudioFormat(o.format)) {
        static const double kbps[3] = {320, 192, 128};
        bits = outDur_ * kbps[q] * 1000;
    } else if (o.format == "gif") {
        const double gw = std::min(fw, o.gifWidth), gh = double(fh) * gw / std::max(1, fw);
        bits = outDur_ * gw * gh * std::min(o.fps, o.gifFps) * 0.5;  // sehr grob: GIF schwankt stark mit dem Inhalt
    } else {
        static const double factor[3] = {1.0, 0.6, 0.38}, bppMin[3] = {0.03, 0.018, 0.011}, bppMax[3] = {0.25, 0.12, 0.07};
        const double pxRate = double(fw) * fh * o.fps;
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
                                 .arg(fw).arg(fh),
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
    if (const ExportPreset* p = currentPreset()) {
        ExportPreset pp = *p;
        pp.fit = fit_->currentData().toInt();
        ExportOptions o = planPreset(pp, srcW_, srcH_, srcFps_, outDur_, hasAudio_).o;
        o.encoder = (o.format == "gif" || isAudioFormat(o.format) || o.videoKbps > 0) ? QString("libx264") : detectEncoder();
        return o;
    }
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
