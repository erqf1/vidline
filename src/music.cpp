#include "music.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
#include "i18n.h"
#include "icons.h"
#include "model.h"
#include "theme.h"

// ---------------------------------------------------------------- Pfade
QString Song::name() const { return title.isEmpty() ? QFileInfo(path).completeBaseName() : title; }

QString musicDataDir() {
    QString d = qEnvironmentVariable("VIDLINE_DATA");
    if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return d;
}

QString musicDownloadDir() {
    QString d = QSettings().value("music/downloads").toString();
    if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/Vidline";
    return d;
}

QStringList audioPatterns() {
    return {"*.mp3", "*.m4a", "*.aac", "*.flac", "*.wav", "*.ogg", "*.oga", "*.opus", "*.wma", "*.aiff", "*.aif",
            "*.ape", "*.wv", "*.mka"};
}

static QString libFile() { return musicDataDir() + "/music/library.json"; }
static QString coverDir() { return musicDataDir() + "/music/covers"; }
static QString normPath(const QString& p) { return QDir::cleanPath(QFileInfo(p).absoluteFilePath()); }

static bool isAudio(const QString& path) {
    const QString ext = "*." + QFileInfo(path).suffix().toLower();
    return audioPatterns().contains(ext);
}

// ---------------------------------------------------------------- Bibliothek
MusicLibrary::MusicLibrary(QObject* parent) : QObject(parent) {
    saveTimer_ = new QTimer(this);
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(400);
    connect(saveTimer_, &QTimer::timeout, this, [this] {
        save();
        emit changed();
    });
    load();
}

MusicLibrary::~MusicLibrary() {
    for (QProcess* p : running_) {
        p->disconnect();
        p->kill();
        p->waitForFinished(1000);
    }
    if (saveTimer_->isActive()) save();
}

const Song* MusicLibrary::find(const QString& path) const {
    for (const Song& s : songs_)
        if (s.path == path) return &s;
    return nullptr;
}

const Playlist* MusicLibrary::playlist(const QString& id) const {
    for (const Playlist& p : lists_)
        if (p.id == id) return &p;
    return nullptr;
}

QString MusicLibrary::coverFile(const Song& s) const { return s.cover.isEmpty() ? QString() : coverDir() + "/" + s.cover; }

void MusicLibrary::load() {
    QFile f(libFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (const QJsonValue& v : root.value("songs").toArray()) {
        const QJsonObject o = v.toObject();
        Song s;
        s.path = o.value("path").toString();
        s.title = o.value("title").toString();
        s.artist = o.value("artist").toString();
        s.album = o.value("album").toString();
        s.track = o.value("track").toString();
        s.duration = o.value("duration").toDouble();
        s.added = qint64(o.value("added").toDouble());
        s.cover = o.value("cover").toString();
        if (!s.path.isEmpty()) songs_.push_back(s);
    }
    for (const QJsonValue& v : root.value("playlists").toArray()) {
        const QJsonObject o = v.toObject();
        Playlist p;
        p.id = o.value("id").toString();
        p.name = o.value("name").toString();
        for (const QJsonValue& x : o.value("paths").toArray()) p.paths << x.toString();
        if (!p.id.isEmpty()) lists_.push_back(p);
    }
}

void MusicLibrary::save() {
    QJsonArray songs, lists;
    for (const Song& s : songs_)
        songs.append(QJsonObject{{"path", s.path}, {"title", s.title}, {"artist", s.artist}, {"album", s.album},
                                 {"track", s.track}, {"duration", s.duration}, {"added", double(s.added)}, {"cover", s.cover}});
    for (const Playlist& p : lists_)
        lists.append(QJsonObject{{"id", p.id}, {"name", p.name}, {"paths", QJsonArray::fromStringList(p.paths)}});
    QDir().mkpath(QFileInfo(libFile()).absolutePath());
    // QSaveFile: erst fertig schreiben, dann austauschen - ein Absturz zerstört nie die Bibliothek
    QSaveFile f(libFile());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{{"version", 1}, {"songs", songs}, {"playlists", lists}}).toJson(QJsonDocument::Compact));
    f.commit();
}

void MusicLibrary::touch() { saveTimer_->start(); }

void MusicLibrary::import(const QStringList& paths, const QString& toPlaylist) {
    QStringList files;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        if (fi.isDir()) {
            QDirIterator it(fi.absoluteFilePath(), audioPatterns(), QDir::Files, QDirIterator::Subdirectories);
            QStringList found;
            while (it.hasNext()) found << normPath(it.next());
            std::sort(found.begin(), found.end(), [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
            files << found;
        } else if (fi.isFile() && isAudio(p)) {
            files << normPath(p);
        }
    }
    QStringList known;
    for (const QString& f : files) {
        if (find(f)) {
            known << f;
            continue;
        }
        bool queued = false;
        for (const Job& j : queue_) queued = queued || j.path == f;
        if (queued) continue;
        queue_.push_back({f, toPlaylist, false});
        ++total_;
    }
    if (!toPlaylist.isEmpty() && !known.isEmpty()) addToPlaylist(toPlaylist, known);
    emit progress(done_, total_);
    scanNext();
}

void MusicLibrary::refresh(const QString& path) {
    if (!find(path)) return;
    queue_.push_back({path, QString(), true});
    ++total_;
    scanNext();
}

// Zwei ffmpeg gleichzeitig: Angaben, Länge und ein kleines Cover in einem Durchgang
void MusicLibrary::scanNext() {
    while (running_.size() < 2 && !queue_.isEmpty()) {
        const Job job = queue_.takeFirst();
        QDir().mkpath(coverDir());
        const QString name = QString::fromLatin1(QCryptographicHash::hash(job.path.toUtf8(), QCryptographicHash::Sha1).toHex().left(20)) + ".jpg";
        const QString out = coverDir() + "/" + name;
        QFile::remove(out);
        auto* p = new QProcess(this);
        running_ << p;
        connect(p, &QProcess::finished, this, [this, p, job, name, out] {
            running_.removeAll(p);
            const QString err = QString::fromUtf8(p->readAllStandardError());
            p->deleteLater();
            scanned(job, err, QFileInfo(out).size() > 0 ? name : QString());
            ++done_;
            if (queue_.isEmpty() && running_.isEmpty()) done_ = total_ = 0;
            emit progress(done_, total_);
            scanNext();
        });
        // "0:v:0?": nur wenn ein Bild eingebettet ist; ohne Bild bricht ffmpeg ab, die Angaben stehen trotzdem da
        p->start(ffmpegPath(), {"-hide_banner", "-nostdin", "-y", "-i", job.path, "-map", "0:v:0?", "-frames:v", "1", "-an",
                                "-vf", "scale=240:240:force_original_aspect_ratio=increase,crop=240:240", "-q:v", "4", out});
    }
}

void MusicLibrary::scanned(const Job& job, const QString& err, const QString& coverName) {
    if (!err.contains("Input #0")) return;  // nicht lesbar: nicht aufnehmen
    Song s;
    s.path = job.path;
    s.added = QDateTime::currentMSecsSinceEpoch();
    const auto d = QRegularExpression("Duration: (\\d+):(\\d+):(\\d+(?:\\.\\d+)?)").match(err);
    if (d.hasMatch()) s.duration = d.captured(1).toInt() * 3600 + d.captured(2).toInt() * 60 + d.captured(3).toDouble();
    // Angaben stehen vor "Duration:", bei ogg und opus an der Tonspur; die der Bildspur gehören zum Cover
    bool use = true;
    QString albumArtist;
    for (const QString& line : err.split('\n')) {
        const QString t = line.trimmed();
        if (t.startsWith("Output #")) break;
        if (t.startsWith("Stream #")) use = t.contains(": Audio:");
        if (!use) continue;
        const int colon = t.indexOf(':');
        if (colon < 0) continue;
        const QString key = t.left(colon).trimmed().toLower(), val = t.mid(colon + 1).trimmed();
        if (val.isEmpty()) continue;
        auto take = [&](QString& field) { if (field.isEmpty()) field = val; };
        if (key == "title") take(s.title);
        else if (key == "artist") take(s.artist);
        else if (key == "album") take(s.album);
        else if (key == "album_artist") take(albumArtist);
        else if (key == "track") take(s.track);
    }
    if (s.artist.isEmpty()) s.artist = albumArtist;
    // Ohne Angaben: "Interpret - Titel" im Dateinamen
    if (s.title.isEmpty() && s.artist.isEmpty()) {
        const QString base = QFileInfo(s.path).completeBaseName();
        const int dash = base.indexOf(" - ");
        if (dash > 0) {
            s.artist = base.left(dash).trimmed().remove(QRegularExpression("^\\d+\\s*[.-]?\\s*"));
            s.title = base.mid(dash + 3).trimmed();
        }
    }
    s.cover = coverName;
    bool replaced = false;
    for (Song& old : songs_)
        if (old.path == s.path) {
            s.added = old.added;
            old = s;
            replaced = true;
        }
    if (!replaced) songs_.push_back(s);
    if (!job.playlist.isEmpty()) {
        for (Playlist& p : lists_)
            if (p.id == job.playlist && !p.paths.contains(s.path)) p.paths << s.path;
    }
    touch();
}

void MusicLibrary::remove(const QStringList& paths) {
    for (const QString& path : paths) {
        for (int i = songs_.size() - 1; i >= 0; --i)
            if (songs_[i].path == path) {
                if (!songs_[i].cover.isEmpty()) QFile::remove(coverFile(songs_[i]));
                songs_.removeAt(i);
            }
        for (Playlist& p : lists_) p.paths.removeAll(path);
    }
    touch();
}

QString MusicLibrary::createPlaylist(const QString& name) {
    Playlist p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.name = name;
    lists_.push_back(p);
    touch();
    return p.id;
}

void MusicLibrary::renamePlaylist(const QString& id, const QString& name) {
    for (Playlist& p : lists_)
        if (p.id == id) p.name = name;
    touch();
}

void MusicLibrary::deletePlaylist(const QString& id) {
    lists_.removeIf([&](const Playlist& p) { return p.id == id; });
    touch();
}

void MusicLibrary::addToPlaylist(const QString& id, const QStringList& paths) {
    for (Playlist& p : lists_)
        if (p.id == id)
            for (const QString& s : paths)
                if (!p.paths.contains(s)) p.paths << s;
    touch();
}

void MusicLibrary::removeFromPlaylist(const QString& id, int index) {
    for (Playlist& p : lists_)
        if (p.id == id && index >= 0 && index < p.paths.size()) p.paths.removeAt(index);
    touch();
}

void MusicLibrary::movePlaylistItem(const QString& id, int from, int to) {
    for (Playlist& p : lists_)
        if (p.id == id && from >= 0 && from < p.paths.size() && to >= 0 && to < p.paths.size()) p.paths.move(from, to);
    touch();
}

// ---------------------------------------------------------------- Liste: eine Zeile pro Song
namespace {
enum Role { PathRole = Qt::UserRole, TitleRole, SubRole, DurRole, MissingRole };

class SongDelegate : public QStyledItemDelegate {
public:
    SongDelegate(const QString* playing, QObject* parent) : QStyledItemDelegate(parent), playing_(playing) {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(200, 54); }
    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override {
        const Theme& th = currentTheme();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRect r = o.rect.adjusted(1, 1, -1, -1);
        if (o.state & QStyle::State_Selected) {
            QColor c = th.accent;
            c.setAlpha(70);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawRoundedRect(r, 8, 8);
        } else if (o.state & QStyle::State_MouseOver) {
            p->setPen(Qt::NoPen);
            p->setBrush(th.hover.isValid() ? th.hover : th.border);
            p->drawRoundedRect(r, 8, 8);
        }
        const QRect img(r.left() + 6, r.center().y() - 20, 40, 40);
        const QPixmap pm = i.data(Qt::DecorationRole).value<QPixmap>();
        QPainterPath clip;
        clip.addRoundedRect(img, 6, 6);
        p->setClipPath(clip);
        if (!pm.isNull()) p->drawPixmap(img, pm);
        p->setClipping(false);
        const bool playing = playing_ && *playing_ == i.data(PathRole).toString();
        const bool missing = i.data(MissingRole).toBool();
        const QString dur = i.data(DurRole).toString();
        QFont f = o.font;
        const QFontMetrics fmSmall(f);
        const int durW = fmSmall.horizontalAdvance(dur) + 10;
        const int x = img.right() + 12, w = r.right() - x - durW;
        f.setWeight(QFont::DemiBold);
        p->setFont(f);
        p->setPen(playing ? th.accent : (missing ? th.muted : th.text));
        const QFontMetrics fm(f);
        p->drawText(QRect(x, r.top() + 8, w, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(i.data(TitleRole).toString(), Qt::ElideRight, w));
        f.setWeight(QFont::Normal);
        f.setPointSizeF(f.pointSizeF() * 0.92);
        p->setFont(f);
        p->setPen(th.muted);
        const QFontMetrics fs(f);
        p->drawText(QRect(x, r.bottom() - 8 - fs.height(), w, fs.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    fs.elidedText(i.data(SubRole).toString(), Qt::ElideRight, w));
        p->drawText(QRect(r.right() - durW, r.top(), durW - 6, r.height()), Qt::AlignRight | Qt::AlignVCenter, dur);
        p->restore();
    }

private:
    const QString* playing_;
};

QString mmss(double s) {
    const int t = int(s + 0.5);
    return t >= 3600 ? QString("%1:%2:%3").arg(t / 3600).arg(t / 60 % 60, 2, 10, QChar('0')).arg(t % 60, 2, 10, QChar('0'))
                     : QString("%1:%2").arg(t / 60).arg(t % 60, 2, 10, QChar('0'));
}

QString keyOf(const QString& s) { return s.trimmed().toCaseFolded(); }
}  // namespace

// ---------------------------------------------------------------- Bereich links
MusicPanel::MusicPanel(MusicLibrary* lib, QWidget* parent) : QFrame(parent), lib_(lib) {
    setObjectName("card");
    setAcceptDrops(true);
    setMinimumWidth(270);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(12, 12, 12, 10);
    v->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setSpacing(6);
    title_ = new QLabel;
    title_->setObjectName("title");
    head->addWidget(title_);
    head->addStretch();
    import_ = new QPushButton;
    import_->setCursor(Qt::PointingHandCursor);
    auto* importMenu = new QMenu(import_);
    connect(importMenu, &QMenu::aboutToShow, this, [this, importMenu] {
        importMenu->clear();
        importMenu->addAction(T("lib_import_files"), this, [this] {
            const QStringList f = QFileDialog::getOpenFileNames(this, T("import"), QString(),
                                                                T("plugin_music") + " (" + audioPatterns().join(' ') + ");;*.*");
            if (!f.isEmpty()) lib_->import(f);
        });
        importMenu->addAction(T("lib_import_folder"), this, [this] {
            const QString d = QFileDialog::getExistingDirectory(this, T("import"));
            if (!d.isEmpty()) lib_->import({d});
        });
    });
    import_->setMenu(importMenu);
    import_->setStyleSheet("QPushButton::menu-indicator { image: none; width: 0; }");
    link_ = new QPushButton;
    link_->setObjectName("primary");
    link_->setCursor(Qt::PointingHandCursor);
    connect(link_, &QPushButton::clicked, this, &MusicPanel::downloadRequested);
    head->addWidget(import_);
    head->addWidget(link_);
    v->addLayout(head);

    search_ = new QLineEdit;
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this, [this] { rebuild(); });
    v->addWidget(search_);

    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(4);
    for (int i = 0; i < 4; ++i) {
        tabs_[i] = new QPushButton;
        tabs_[i]->setCheckable(true);
        tabs_[i]->setCursor(Qt::PointingHandCursor);
        tabs_[i]->setFocusPolicy(Qt::NoFocus);
        connect(tabs_[i], &QPushButton::clicked, this, [this, i] { showView(i); });
        tabs->addWidget(tabs_[i], 1);
    }
    v->addLayout(tabs);

    crumbRow_ = new QWidget;
    auto* cr = new QHBoxLayout(crumbRow_);
    cr->setContentsMargins(0, 0, 0, 0);
    cr->setSpacing(6);
    back_ = new QPushButton(QString::fromUtf8("‹"));
    back_->setFixedWidth(34);
    back_->setCursor(Qt::PointingHandCursor);
    connect(back_, &QPushButton::clicked, this, [this] {
        groupKind_.clear();
        groupKey_.clear();
        rebuild();
    });
    crumb_ = new QLabel;
    crumb_->setObjectName("title");
    crumb_->setMinimumWidth(10);
    playAll_ = new QPushButton;
    playAll_->setCursor(Qt::PointingHandCursor);
    connect(playAll_, &QPushButton::clicked, this, [this] {
        const QStringList p = shownPaths();
        if (!p.isEmpty()) emit play(p, 0);
    });
    cr->addWidget(back_);
    cr->addWidget(crumb_, 1);
    cr->addWidget(playAll_);
    v->addWidget(crumbRow_);

    stack_ = new QStackedWidget;
    songs_ = new QListWidget;
    songs_->setItemDelegate(new SongDelegate(&nowPlaying_, songs_));
    songs_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    songs_->setUniformItemSizes(true);
    songs_->setMouseTracking(true);
    songs_->setContextMenuPolicy(Qt::CustomContextMenu);
    songs_->setStyleSheet("QListWidget::item { padding: 0; }");
    connect(songs_, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) { emit play(shownPaths(), songs_->row(it)); });
    connect(songs_, &QListWidget::customContextMenuRequested, this, &MusicPanel::songMenu);
    grid_ = new QListWidget;
    grid_->setViewMode(QListView::IconMode);
    grid_->setIconSize(QSize(112, 112));
    grid_->setGridSize(QSize(132, 168));
    grid_->setResizeMode(QListView::Adjust);
    grid_->setMovement(QListView::Static);
    grid_->setWordWrap(true);
    grid_->setUniformItemSizes(true);
    grid_->setContextMenuPolicy(Qt::CustomContextMenu);
    grid_->setStyleSheet("QListWidget::item { padding: 4px; }");
    connect(grid_, &QListWidget::itemActivated, this, &MusicPanel::gridActivated);
    connect(grid_, &QListWidget::itemClicked, this, &MusicPanel::gridActivated);
    connect(grid_, &QListWidget::customContextMenuRequested, this, &MusicPanel::gridMenu);
    empty_ = new QLabel;
    empty_->setObjectName("hint");
    empty_->setWordWrap(true);
    empty_->setAlignment(Qt::AlignCenter);
    stack_->addWidget(songs_);
    stack_->addWidget(grid_);
    stack_->addWidget(empty_);
    v->addWidget(stack_, 1);

    progress_ = new QProgressBar;
    progress_->setTextVisible(false);
    progress_->setFixedHeight(6);
    progress_->hide();
    v->addWidget(progress_);
    auto* foot = new QHBoxLayout;
    count_ = new QLabel;
    count_->setObjectName("hint");
    sort_ = new QComboBox;
    sort_->addItems({QString(), QString(), QString(), QString()});
    sort_->setCurrentIndex(QSettings().value("music/sort", 0).toInt());
    connect(sort_, &QComboBox::currentIndexChanged, this, [this](int i) {
        QSettings().setValue("music/sort", i);
        rebuild();
    });
    foot->addWidget(count_, 1);
    foot->addWidget(sort_);
    v->addLayout(foot);

    rebuildTimer_ = new QTimer(this);
    rebuildTimer_->setSingleShot(true);
    rebuildTimer_->setInterval(120);
    connect(rebuildTimer_, &QTimer::timeout, this, &MusicPanel::rebuild);
    connect(lib_, &MusicLibrary::changed, this, [this] { rebuildTimer_->start(); });
    connect(lib_, &MusicLibrary::progress, this, [this](int done, int total) {
        progress_->setVisible(total > 0);
        progress_->setRange(0, std::max(1, total));
        progress_->setValue(done);
        if (total > 0) count_->setText(T("lib_scanning").arg(done).arg(total));
        else rebuildTimer_->start();
    });
    view_ = QSettings().value("music/view", 0).toInt();
    retranslate();
}

void MusicPanel::retranslate() {
    title_->setText(T("plugin_music"));
    import_->setText(T("import"));
    link_->setText(T("lib_link"));
    search_->setPlaceholderText(T("lib_search"));
    const char* tabs[4] = {"lib_songs", "lib_albums", "lib_artists", "lib_playlists"};
    for (int i = 0; i < 4; ++i) tabs_[i]->setText(T(tabs[i]));
    playAll_->setText(T("lib_play_all"));
    const char* sorts[4] = {"lib_sort_added", "tag_title", "tag_artist", "tag_album"};
    for (int i = 0; i < 4; ++i) sort_->setItemText(i, T(sorts[i]));
    back_->setToolTip(T("lib_back"));
    rebuild();
}

void MusicPanel::setNowPlaying(const QString& path) {
    nowPlaying_ = path;
    songs_->viewport()->update();
}

void MusicPanel::showView(int view) {
    view_ = std::clamp(view, 0, 3);
    QSettings().setValue("music/view", view_);
    groupKind_.clear();
    groupKey_.clear();
    rebuild();
}

void MusicPanel::openGroup(const QString& kind, const QString& key) {
    groupKind_ = kind;
    groupKey_ = key;
    rebuild();
}

void MusicPanel::setSearch(const QString& text) { search_->setText(text); }

int MusicPanel::visibleCount() const { return stack_->currentWidget() == songs_ ? songs_->count() : grid_->count(); }

QPixmap MusicPanel::placeholder(int size, bool round) {
    const Theme& th = currentTheme();
    const QString key = QString("~%1%2%3").arg(size).arg(round).arg(th.panel2.name());
    if (covers_.contains(key)) return covers_[key];
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(th.panel2.isValid() ? th.panel2 : th.border);
    if (round) p.drawEllipse(pm.rect());
    else p.drawRoundedRect(pm.rect(), size / 8, size / 8);
    const int g = size * 2 / 5;
    p.drawPixmap((size - g) / 2, (size - g) / 2, makeIcon(Ic::Music, th.muted).pixmap(g, g));
    covers_[key] = pm;
    return pm;
}

QPixmap MusicPanel::cover(const Song* s, int size) {
    if (!s || s->cover.isEmpty()) return placeholder(size, false);
    const QString key = QString("%1@%2").arg(s->cover).arg(size);
    if (!covers_.contains(key)) {
        QPixmap pm(lib_->coverFile(*s));
        covers_[key] = pm.isNull() ? placeholder(size, false)
                                   : pm.scaled(size * 2, size * 2, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    return covers_[key];
}

QList<const Song*> MusicPanel::groupSongs(const QString& kind, const QString& key) const {
    QList<const Song*> out;
    if (kind == "list") {
        if (const Playlist* pl = lib_->playlist(key))
            for (const QString& p : pl->paths)
                if (const Song* s = lib_->find(p)) out << s;
        return out;
    }
    for (const Song& s : lib_->songs()) {
        if (kind == "album" && keyOf(s.album) == key) out << &s;
        else if (kind == "artist" && keyOf(s.artist) == key) out << &s;
        else if (kind.isEmpty()) out << &s;
    }
    return out;
}

void MusicPanel::rebuild() {
    for (int i = 0; i < 4; ++i) tabs_[i]->setChecked(i == view_);
    const bool inGroup = !groupKind_.isEmpty();
    crumbRow_->setVisible(inGroup);
    if (inGroup || view_ == Songs) fillSongs();
    else fillGrid();
}

void MusicPanel::fillSongs() {
    QList<const Song*> list = groupSongs(groupKind_, groupKey_);
    if (groupKind_ == "album") {
        const Song* first = list.isEmpty() ? nullptr : list.first();
        crumb_->setText(first && !first->album.isEmpty() ? first->album : T("lib_unknown_album"));
    } else if (groupKind_ == "artist") {
        const Song* first = list.isEmpty() ? nullptr : list.first();
        crumb_->setText(first && !first->artist.isEmpty() ? first->artist : T("lib_unknown_artist"));
    } else if (groupKind_ == "list") {
        const Playlist* pl = lib_->playlist(groupKey_);
        crumb_->setText(pl ? pl->name : QString());
    }
    const QString q = search_->text().trimmed();
    if (!q.isEmpty())
        list.erase(std::remove_if(list.begin(), list.end(), [&](const Song* s) {
                       return !(s->name().contains(q, Qt::CaseInsensitive) || s->artist.contains(q, Qt::CaseInsensitive) ||
                                s->album.contains(q, Qt::CaseInsensitive));
                   }),
                   list.end());
    // Playlists behalten ihre Reihenfolge, Alben laufen nach Titelnummer
    if (groupKind_ == "album") {
        std::stable_sort(list.begin(), list.end(), [](const Song* a, const Song* b) { return a->track.toInt() < b->track.toInt(); });
    } else if (groupKind_ != "list") {
        const int mode = sort_->currentIndex();
        auto text = [mode](const Song* s) { return mode == 1 ? s->name() : mode == 2 ? s->artist : s->album; };
        std::stable_sort(list.begin(), list.end(), [&](const Song* a, const Song* b) {
            if (mode == 0) return a->added > b->added;
            const int c = QString::localeAwareCompare(text(a), text(b));
            return c != 0 ? c < 0 : QString::localeAwareCompare(a->name(), b->name()) < 0;
        });
    }
    songs_->setUpdatesEnabled(false);
    songs_->clear();
    double total = 0;
    for (const Song* s : list) {
        auto* it = new QListWidgetItem;
        it->setData(PathRole, s->path);
        it->setData(TitleRole, s->name());
        QStringList sub;
        if (!s->artist.isEmpty()) sub << s->artist;
        if (!s->album.isEmpty() && groupKind_ != "album") sub << s->album;
        const bool missing = !QFileInfo::exists(s->path);
        it->setData(SubRole, missing ? T("lib_missing") : sub.join(QString::fromUtf8(" · ")));
        it->setData(DurRole, s->duration > 0 ? mmss(s->duration) : QString());
        it->setData(MissingRole, missing);
        it->setData(Qt::DecorationRole, cover(s, 40));
        it->setToolTip(s->path);
        songs_->addItem(it);
        total += s->duration;
    }
    songs_->setUpdatesEnabled(true);
    const bool none = list.isEmpty();
    if (none) empty_->setText(lib_->songs().isEmpty() ? T("lib_empty") : T("lib_nothing"));
    stack_->setCurrentWidget(none ? static_cast<QWidget*>(empty_) : songs_);
    if (!lib_->busy()) count_->setText(T("lib_count").arg(list.size()).arg(mmss(total)));
    sort_->setVisible(groupKind_.isEmpty());
    playAll_->setEnabled(!none);
}

void MusicPanel::fillGrid() {
    const QString q = search_->text().trimmed();
    grid_->setUpdatesEnabled(false);
    grid_->clear();
    if (view_ == Lists) {
        auto* add = new QListWidgetItem(placeholder(112, false), "+ " + T("lib_new_playlist"));
        add->setData(Qt::UserRole, "+");
        grid_->addItem(add);
        for (const Playlist& pl : lib_->playlists()) {
            if (!q.isEmpty() && !pl.name.contains(q, Qt::CaseInsensitive)) continue;
            const Song* first = nullptr;
            for (const QString& p : pl.paths)
                if (const Song* s = lib_->find(p); s && !s->cover.isEmpty()) { first = s; break; }
            auto* it = new QListWidgetItem(cover(first, 112), pl.name + "\n" + T("lib_n").arg(pl.paths.size()));
            it->setData(Qt::UserRole, pl.id);
            grid_->addItem(it);
        }
    } else {
        // Alben und Interpreten: eine Kachel pro Name, Cover vom ersten Song mit Bild
        struct Group { QString label, sub; const Song* cover = nullptr; int count = 0; };
        QMap<QString, Group> groups;
        for (const Song& s : lib_->songs()) {
            const QString name = view_ == Albums ? s.album : s.artist;
            const QString key = keyOf(name);
            Group& g = groups[key];
            if (g.label.isEmpty()) g.label = name.isEmpty() ? T(view_ == Albums ? "lib_unknown_album" : "lib_unknown_artist") : name;
            if (g.sub.isEmpty() && view_ == Albums) g.sub = s.artist;
            if (!g.cover && !s.cover.isEmpty()) g.cover = &s;
            ++g.count;
        }
        QList<QPair<QString, Group>> sorted;
        for (auto it = groups.begin(); it != groups.end(); ++it) sorted << qMakePair(it.key(), it.value());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            if (a.first.isEmpty() != b.first.isEmpty()) return b.first.isEmpty();  // "Unbekannt" ans Ende
            return QString::localeAwareCompare(a.second.label, b.second.label) < 0;
        });
        for (const auto& [key, g] : sorted) {
            if (!q.isEmpty() && !g.label.contains(q, Qt::CaseInsensitive) && !g.sub.contains(q, Qt::CaseInsensitive)) continue;
            QPixmap pm = g.cover ? cover(g.cover, 112) : placeholder(112, view_ == Artists);
            if (view_ == Artists && g.cover) {
                // Interpreten rund, wie man es kennt
                QPixmap r(pm.size());
                r.fill(Qt::transparent);
                QPainter p(&r);
                p.setRenderHint(QPainter::Antialiasing);
                QPainterPath c;
                c.addEllipse(QRectF(r.rect()));
                p.setClipPath(c);
                p.drawPixmap(0, 0, pm);
                pm = r;
            }
            auto* it = new QListWidgetItem(QIcon(pm), g.label + "\n" + (view_ == Albums && !g.sub.isEmpty() ? g.sub : T("lib_n").arg(g.count)));
            it->setData(Qt::UserRole, key);
            it->setToolTip(g.label);
            grid_->addItem(it);
        }
    }
    grid_->setUpdatesEnabled(true);
    const bool none = grid_->count() == 0;
    if (none) empty_->setText(lib_->songs().isEmpty() ? T("lib_empty") : T("lib_nothing"));
    stack_->setCurrentWidget(none ? static_cast<QWidget*>(empty_) : grid_);
    if (!lib_->busy()) count_->setText(T("lib_count").arg(lib_->songs().size()).arg([this] {
        double t = 0;
        for (const Song& s : lib_->songs()) t += s.duration;
        return mmss(t);
    }()));
    sort_->hide();
}

QStringList MusicPanel::shownPaths() const {
    QStringList out;
    for (int i = 0; i < songs_->count(); ++i)
        if (!songs_->item(i)->data(MissingRole).toBool()) out << songs_->item(i)->data(PathRole).toString();
    return out;
}

QStringList MusicPanel::selectedPaths() const {
    QStringList out;
    for (int i = 0; i < songs_->count(); ++i)
        if (songs_->item(i)->isSelected()) out << songs_->item(i)->data(PathRole).toString();
    return out;
}

void MusicPanel::gridActivated(QListWidgetItem* it) {
    if (!it) return;
    const QString key = it->data(Qt::UserRole).toString();
    if (view_ == Lists && key == "+") {
        newPlaylist({});
        return;
    }
    openGroup(view_ == Albums ? "album" : view_ == Artists ? "artist" : "list", key);
}

void MusicPanel::newPlaylist(const QStringList& paths) {
    bool ok = false;
    const QString name = QInputDialog::getText(this, T("lib_new_playlist"), T("lib_playlist_name"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const QString id = lib_->createPlaylist(name);
    if (!paths.isEmpty()) lib_->addToPlaylist(id, paths);
}

static void revealFile(const QString& path) {
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", {"-R", path});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void MusicPanel::songMenu(const QPoint& pos) {
    QListWidgetItem* at = songs_->itemAt(pos);
    if (!at) return;
    if (!at->isSelected()) {
        songs_->clearSelection();
        at->setSelected(true);
    }
    const QStringList sel = selectedPaths();
    const int row = songs_->row(at);
    QMenu m(this);
    m.addAction(T("lib_play"), this, [this, row] { emit play(shownPaths(), row); });
    m.addAction(T("lib_play_next"), this, [this, sel] { emit playNext(sel); });
    m.addAction(T("lib_enqueue"), this, [this, sel] { emit enqueue(sel); });
    QMenu* add = m.addMenu(T("lib_add_to"));
    add->addAction(T("lib_new_playlist") + QString::fromUtf8("…"), this, [this, sel] { newPlaylist(sel); });
    if (!lib_->playlists().isEmpty()) add->addSeparator();
    for (const Playlist& pl : lib_->playlists()) add->addAction(pl.name, this, [this, id = pl.id, sel] { lib_->addToPlaylist(id, sel); });
    if (groupKind_ == "list" && search_->text().isEmpty()) {
        m.addSeparator();
        const QString id = groupKey_;
        if (row > 0) m.addAction(T("lib_up"), this, [this, id, row] { lib_->movePlaylistItem(id, row, row - 1); });
        if (row + 1 < songs_->count()) m.addAction(T("lib_down"), this, [this, id, row] { lib_->movePlaylistItem(id, row, row + 1); });
        m.addAction(T("lib_remove_pl"), this, [this, id, row] { lib_->removeFromPlaylist(id, row); });
    }
    m.addSeparator();
    m.addAction(T("lib_show"), this, [path = at->data(PathRole).toString()] { revealFile(path); });
    m.addAction(T("lib_remove"), this, [this, sel] { lib_->remove(sel); });
    m.exec(songs_->viewport()->mapToGlobal(pos));
}

void MusicPanel::gridMenu(const QPoint& pos) {
    QListWidgetItem* at = grid_->itemAt(pos);
    if (!at) return;
    const QString key = at->data(Qt::UserRole).toString();
    if (key == "+") return;
    const QString kind = view_ == Albums ? "album" : view_ == Artists ? "artist" : "list";
    QStringList paths;
    for (const Song* s : groupSongs(kind, key))
        if (QFileInfo::exists(s->path)) paths << s->path;
    QMenu m(this);
    m.addAction(T("lib_play"), this, [this, paths] { if (!paths.isEmpty()) emit play(paths, 0); });
    m.addAction(T("lib_enqueue"), this, [this, paths] { emit enqueue(paths); });
    if (kind == "list") {
        m.addSeparator();
        m.addAction(T("lib_rename"), this, [this, key] {
            const Playlist* pl = lib_->playlist(key);
            bool ok = false;
            const QString name = QInputDialog::getText(this, T("lib_rename"), T("lib_playlist_name"), QLineEdit::Normal,
                                                       pl ? pl->name : QString(), &ok).trimmed();
            if (ok && !name.isEmpty()) lib_->renamePlaylist(key, name);
        });
        m.addAction(T("lib_delete_pl"), this, [this, key] {
            const Playlist* pl = lib_->playlist(key);
            if (QMessageBox::question(this, T("lib_delete_pl"), T("lib_delete_q").arg(pl ? pl->name : QString())) == QMessageBox::Yes)
                lib_->deletePlaylist(key);
        });
    }
    m.exec(grid_->viewport()->mapToGlobal(pos));
}

void MusicPanel::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MusicPanel::dropEvent(QDropEvent* e) {
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) paths << u.toLocalFile();
    if (paths.isEmpty()) return;
    e->acceptProposedAction();
    lib_->import(paths, groupKind_ == "list" ? groupKey_ : QString());
}

// ---------------------------------------------------------------- Herunterladen per Link (yt-dlp)
QString ytDlpPath() {
#ifdef Q_OS_WIN
    return musicDataDir() + "/plugins/yt-dlp.exe";
#else
    return musicDataDir() + "/plugins/yt-dlp";
#endif
}

static QString ytDlpUrl() {
    const QString base = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";
#if defined(Q_OS_WIN)
    return base + "yt-dlp.exe";
#elif defined(Q_OS_MAC)
    return base + "yt-dlp_macos";
#else
    return base + (QSysInfo::currentCpuArchitecture().contains("arm") ? "yt-dlp_linux_aarch64" : "yt-dlp_linux");
#endif
}

DownloadDialog::DownloadDialog(MusicLibrary* lib, QWidget* parent) : QDialog(parent), lib_(lib) {
    setWindowTitle(T("dl_title"));
    setMinimumWidth(520);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(10);
    auto* lbl = new QLabel(T("dl_link"));
    v->addWidget(lbl);
    url_ = new QLineEdit;
    url_->setPlaceholderText(T("dl_link_ph"));
    url_->setClearButtonEnabled(true);
    v->addWidget(url_);
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(new QLabel(T("format")));
    fmt_ = new QComboBox;
    fmt_->addItem("MP3", "mp3");
    fmt_->addItem("M4A", "m4a");
    fmt_->addItem("Opus", "opus");
    fmt_->setCurrentIndex(std::max(0, fmt_->findData(QSettings().value("music/dlformat", "mp3"))));
    row->addWidget(fmt_);
    row->addStretch();
    whole_ = new QCheckBox(T("dl_whole"));
    whole_->setChecked(true);
    row->addWidget(whole_);
    v->addLayout(row);
    auto* frow = new QHBoxLayout;
    frow->addWidget(new QLabel(T("folder")));
    dir_ = musicDownloadDir();
    folder_ = new QLabel(QDir::toNativeSeparators(dir_));
    folder_->setObjectName("hint");
    folder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    frow->addWidget(folder_, 1);
    pick_ = new QPushButton(T("browse"));
    connect(pick_, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("folder"), dir_);
        if (d.isEmpty()) return;
        dir_ = d;
        QSettings().setValue("music/downloads", d);
        folder_->setText(QDir::toNativeSeparators(d));
    });
    frow->addWidget(pick_);
    v->addLayout(frow);
    auto* note = new QLabel(T("dl_note"));
    note->setObjectName("hint");
    note->setWordWrap(true);
    v->addWidget(note);
    bar_ = new QProgressBar;
    bar_->setRange(0, 1000);
    bar_->setTextVisible(false);
    bar_->setFixedHeight(8);
    bar_->hide();
    v->addWidget(bar_);
    status_ = new QLabel;
    status_->setWordWrap(true);
    v->addWidget(status_);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(400);
    log_->setFixedHeight(110);
    log_->hide();
    v->addWidget(log_);
    auto* btns = new QHBoxLayout;
    btns->addStretch();
    close_ = new QPushButton(T("close"));
    start_ = new QPushButton(T("dl_start"));
    start_->setObjectName("primary");
    connect(close_, &QPushButton::clicked, this, [this] {
        if (running()) {
            proc_->kill();
            return;
        }
        reject();
    });
    connect(start_, &QPushButton::clicked, this, &DownloadDialog::start);
    connect(url_, &QLineEdit::returnPressed, this, &DownloadDialog::start);
    btns->addWidget(close_);
    btns->addWidget(start_);
    v->addLayout(btns);
}

DownloadDialog::~DownloadDialog() {
    if (proc_) {
        proc_->disconnect();
        proc_->kill();
        proc_->waitForFinished(2000);
    }
}

bool DownloadDialog::running() const { return (proc_ && proc_->state() != QProcess::NotRunning) || !start_->isEnabled(); }
QString DownloadDialog::status() const { return status_->text(); }

void DownloadDialog::startWith(const QString& url, bool wholeList) {
    url_->setText(url);
    whole_->setChecked(wholeList);
    start();
}

void DownloadDialog::setBusy(bool on) {
    start_->setEnabled(!on);
    url_->setEnabled(!on);
    fmt_->setEnabled(!on);
    whole_->setEnabled(!on);
    pick_->setEnabled(!on);
    close_->setText(on ? T("cancel") : T("close"));
    bar_->setVisible(on);
}

void DownloadDialog::start() {
    const QString url = url_->text().trimmed();
    if (url.isEmpty() || !start_->isEnabled()) return;
    if (!QUrl(url).isValid() || !url.contains('.')) {
        status_->setText(T("dl_failed"));
        return;
    }
    QSettings().setValue("music/dlformat", fmt_->currentData());
    files_.clear();
    listTitle_.clear();
    lastError_.clear();
    log_->clear();
    setBusy(true);
    bar_->setRange(0, 0);
    if (QFileInfo(ytDlpPath()).size() > 1000000) maybeUpdate();
    else setup();
}

// yt-dlp von GitHub laden (einmalig), danach prüfen, ob es läuft
void DownloadDialog::setup() {
    if (!net_) net_ = new QNetworkAccessManager(this);
    QDir().mkpath(QFileInfo(ytDlpPath()).absolutePath());
    status_->setText(T("dl_setup").arg("…"));
    QNetworkRequest req{QUrl(ytDlpUrl())};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Vidline");
    req.setTransferTimeout(30000);  // 30 s ohne Daten: abbrechen statt ewig zu warten
    QNetworkReply* r = net_->get(req);
    connect(r, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        status_->setText(T("dl_setup").arg(total > 0 ? QString::number(total / 1048576.0, 'f', 0) : QString::number(got / 1048576.0, 'f', 0)));
        if (total > 0) {
            bar_->setRange(0, 1000);
            bar_->setValue(int(got * 1000 / total));
        }
    });
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        const QByteArray data = r->readAll();
        if (r->error() != QNetworkReply::NoError || data.size() < 1000000) {
            finish(false, r->errorString());
            return;
        }
        QSaveFile f(ytDlpPath());
        if (!f.open(QIODevice::WriteOnly)) {
            finish(false, f.errorString());
            return;
        }
        f.write(data);
        f.commit();
        QFile::setPermissions(ytDlpPath(), QFile::permissions(ytDlpPath()) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
        run();
    });
}

// yt-dlp muss mit den Seiten Schritt halten: höchstens alle 7 Tage auf eine neue Version prüfen
void DownloadDialog::maybeUpdate() {
    QSettings st;
    const qint64 last = st.value("music/ytdlpChecked", 0).toLongLong();
    if (QDateTime::currentSecsSinceEpoch() - last < 7 * 86400) {
        run();
        return;
    }
    st.setValue("music/ytdlpChecked", QDateTime::currentSecsSinceEpoch());
    status_->setText(T("dl_update"));
    auto* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p] {
        p->deleteLater();
        run();
    });
    p->start(ytDlpPath(), {"-U"});
}

void DownloadDialog::run() {
    QDir().mkpath(dir_);
    startedAt_ = QDateTime::currentMSecsSinceEpoch();
    proc_ = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");  // sonst kommen Umlaute in Titeln unter Windows kaputt an
    env.insert("PYTHONUTF8", "1");
    proc_->setProcessEnvironment(env);
    proc_->setProcessChannelMode(QProcess::MergedChannels);
    QStringList a = {"--newline", "--no-colors", "--ignore-errors", "--no-mtime", "--progress",
                     "-x", "--audio-format", fmt_->currentData().toString(), "--audio-quality", "0",
                     "--embed-metadata", "--embed-thumbnail", "--convert-thumbnails", "jpg",
                     "-P", dir_, "-o", "%(title)s.%(ext)s",
                     whole_->isChecked() ? "--yes-playlist" : "--no-playlist",
                     "--progress-template", "download:CLPROG\t%(progress._percent_str)s\t%(info.playlist_index|1)s\t%(info.n_entries|1)s\t%(info.title)s",
                     "--print", "after_move:CLFILE\t%(playlist_title|)s\t%(filepath)s"};
    const QString ff = ffmpegPath();
    if (QFileInfo(ff).isAbsolute()) a << "--ffmpeg-location" << ff;
    a << url_->text().trimmed();
    connect(proc_, &QProcess::readyRead, this, [this] {
        while (proc_->canReadLine()) {
            const QString line = QString::fromUtf8(proc_->readLine()).trimmed();
            if (line.isEmpty()) continue;
            if (line.startsWith("CLPROG\t")) {
                const QStringList f = line.split('\t');
                const double pct = f.value(1).remove('%').trimmed().toDouble();
                const int idx = std::max(1, f.value(2).toInt()), n = std::max(1, f.value(3).toInt());
                bar_->setRange(0, 1000);
                bar_->setValue(int(((idx - 1) + pct / 100.0) / n * 1000));
                status_->setText(T("dl_progress").arg(idx).arg(n).arg(f.value(4)));
            } else if (line.startsWith("CLFILE\t")) {
                const QStringList f = line.split('\t');
                if (listTitle_.isEmpty()) listTitle_ = f.value(1).trimmed();
                if (QFileInfo::exists(f.value(2))) files_ << f.value(2);
            } else {
                log_->appendPlainText(line);
                if (line.startsWith("ERROR")) lastError_ = line;
            }
        }
    });
    connect(proc_, &QProcess::finished, this, [this](int code) {
        proc_->deleteLater();
        proc_ = nullptr;
        // Abgebrochen: angefangene Teile wegräumen
        QDir d(dir_);
        for (const QFileInfo& fi : d.entryInfoList({"*.part", "*.ytdl", "*.part-Frag*"}, QDir::Files))
            if (fi.lastModified().toMSecsSinceEpoch() >= startedAt_ - 2000) QFile::remove(fi.absoluteFilePath());
        if (files_.isEmpty()) {
            finish(false, lastError_.isEmpty() ? QString("yt-dlp: %1").arg(code) : lastError_);
            return;
        }
        QString list;
        if (whole_->isChecked() && files_.size() > 1 && !listTitle_.isEmpty()) {
            for (const Playlist& p : lib_->playlists())
                if (p.name == listTitle_) list = p.id;
            if (list.isEmpty()) list = lib_->createPlaylist(listTitle_);
        }
        lib_->import(files_, list);
        finish(true, T("dl_done").arg(files_.size()));
    });
    proc_->start(ytDlpPath(), a);
}

void DownloadDialog::finish(bool ok, const QString& message) {
    setBusy(false);
    bar_->setRange(0, 1000);
    bar_->setValue(ok ? 1000 : 0);
    status_->setText(ok ? message : T("dl_failed"));
    if (!ok) {
        log_->show();
        if (!message.isEmpty()) log_->appendPlainText(message);
    }
}
