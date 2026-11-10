#include "videodl.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include "model.h"
#include "i18n.h"
#include "music.h"

// ---------------------------------------------------------------- reine Helfer (ohne Netz testbar)
QString vdlSize(qint64 b) {
    if (b <= 0) return QString();
    if (b >= 1024LL * 1024 * 1024) return QString::number(b / 1073741824.0, 'f', 1) + " GB";
    if (b >= 1024 * 1024) return QString::number(qRound(b / 1048576.0)) + " MB";
    return QString::number(std::max<qint64>(1, b / 1024)) + " KB";
}

double vdlParseTime(const QString& s) {
    const QString t = s.trimmed();
    if (t.isEmpty()) return -1;
    double total = 0;
    const QStringList parts = t.split(':');
    if (parts.size() > 3) return -1;
    for (const QString& p : parts) {
        bool ok = false;
        const double v = p.trimmed().toDouble(&ok);
        if (!ok || v < 0) return -1;
        total = total * 60 + v;
    }
    return total;
}

static QString hms(double sec) {
    const int s = int(std::lround(sec));
    return s >= 3600 ? QString("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'))
                     : QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

VdlInfo parseVdlInfo(const QByteArray& json) {
    VdlInfo in;
    // yt-dlp schreibt bei Playlists eine Zeile je Eintrag oder ein ganzes Objekt - wir nehmen das letzte Objekt
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (!doc.isObject()) {
        const QList<QByteArray> lines = json.trimmed().split('\n');
        for (auto it = lines.rbegin(); it != lines.rend() && !doc.isObject(); ++it) doc = QJsonDocument::fromJson(*it);
    }
    if (!doc.isObject()) {
        in.error = err.errorString();
        return in;
    }
    const QJsonObject o = doc.object();
    in.ok = true;
    in.title = o.value("title").toString();
    in.uploader = o.value("uploader").toString(o.value("channel").toString(o.value("playlist_uploader").toString()));
    in.site = o.value("extractor_key").toString(o.value("extractor").toString());
    in.duration = o.value("duration").toDouble();
    // Vorschaubild: das größte JPG/PNG (Qt kann WebP ohne Zusatzmodul nicht lesen)
    auto pickThumb = [](const QJsonArray& th) {
        QString best;
        double bestScore = -1;
        for (const QJsonValue& tv : th) {
            const QJsonObject t = tv.toObject();
            const QString u = t.value("url").toString();
            const QString path = QUrl(u).path().toLower();
            if (!(path.endsWith(".jpg") || path.endsWith(".jpeg") || path.endsWith(".png"))) continue;
            const double score = t.value("width").toDouble() * t.value("height").toDouble() + t.value("preference").toDouble() * 1e-3;
            if (score > bestScore || best.isEmpty()) {
                bestScore = score;
                best = u;
            }
        }
        return best;
    };
    in.thumb = pickThumb(o.value("thumbnails").toArray());
    if (in.thumb.isEmpty()) in.thumb = o.value("thumbnail").toString();
    if (o.value("_type").toString() == "playlist") {
        in.playlist = true;
        const QJsonArray e = o.value("entries").toArray();
        in.count = o.value("playlist_count").toInt(e.size());
        if (in.thumb.isEmpty() && !e.isEmpty()) in.thumb = pickThumb(e.first().toObject().value("thumbnails").toArray());
    }
    const QJsonArray formats = o.value("formats").toArray();
    QSet<int> hs;
    for (const QJsonValue& fv : formats) {
        const QJsonObject f = fv.toObject();
        const QString vc = f.value("vcodec").toString("none"), ac = f.value("acodec").toString("none");
        const QString ext = f.value("ext").toString();
        if (ext == "mhtml") continue;  // Vorschaubilder ("storyboards")
        qint64 size = qint64(f.value("filesize").toDouble(f.value("filesize_approx").toDouble()));
        if (size <= 0 && f.value("tbr").toDouble() > 0 && in.duration > 0) size = qint64(f.value("tbr").toDouble() * 125 * in.duration);
        const int h = f.value("height").toInt();
        if (vc != "none" && h > 0) {
            hs.insert(h);
            in.fpsAt[h] = std::max(in.fpsAt.value(h), int(std::lround(f.value("fps").toDouble())));
            in.videoSize[h] = std::max(in.videoSize.value(h), size);
            if (vc.startsWith("avc1") || vc.startsWith("h264")) {
                in.h264Heights.insert(h);
                in.videoSizeH264[h] = std::max(in.videoSizeH264.value(h), size);
            }
        } else if (vc == "none" && ac != "none") {
            in.audioSize = std::max(in.audioSize, size);
        }
    }
    in.heights = QList<int>(hs.begin(), hs.end());
    std::sort(in.heights.begin(), in.heights.end(), std::greater<int>());
    const QJsonObject subs = o.value("subtitles").toObject();
    for (auto it = subs.begin(); it != subs.end(); ++it)
        if (it.key() != "live_chat") in.subs << it.key();
    const QJsonObject autos = o.value("automatic_captions").toObject();
    // automatische Untertitel gibt es oft in 100+ Sprachen - nur die gängigen anbieten
    static const QStringList common = {"de", "en", "es", "fr", "it", "pt", "nl", "pl", "tr", "ru", "ja", "zh-Hans"};
    for (const QString& l : common)
        if (autos.contains(l) && !in.subs.contains(l)) in.autoSubs << l;
    return in;
}

QStringList vdlArgs(const VdlOptions& o, const QString& url, const QString& ffmpeg) {
    QStringList a = {"--newline", "--no-colors", "--no-mtime", "--progress", "--ignore-errors", "--trim-filenames", "150"};
    const bool audio = o.what == 2;
    const QString res = o.height > 0 ? QString("res:%1").arg(o.height) : QString("res");
    if (audio) {
        a << "-f" << "ba/b" << "-x" << "--audio-format" << o.audioFmt;
        a << "--audio-quality" << (o.audioKbps > 0 ? QString("%1K").arg(o.audioKbps) : QString("0"));
    } else {
        a << "-f" << (o.what == 1 ? "bv*/b" : "bv*+ba/b");
        // Sortierung statt starrer Filter: nimmt die gewünschte Auflösung oder die nächstkleinere
        if (o.compat) a << "-S" << QString("vcodec:h264,%1,fps,acodec:aac").arg(res);
        else if (o.container == "webm") a << "-S" << QString("%1,fps,vcodec:vp9,acodec:opus").arg(res);
        else a << "-S" << QString("%1,fps").arg(res);
        const QString c = o.compat ? "mp4" : o.container;
        a << "--merge-output-format" << c << "--remux-video" << c;
    }
    if (o.embed) {
        a << "--embed-metadata" << "--embed-chapters";
        // WebM kann kein Cover tragen
        if (audio || o.container != "webm") a << "--embed-thumbnail" << "--convert-thumbnails" << "jpg";
    }
    if (!o.subLang.isEmpty() && !audio) {
        a << (o.autoSub ? "--write-auto-subs" : "--write-subs") << "--sub-langs" << o.subLang << "--embed-subs";
        if (o.container == "mp4" || o.compat) a << "--convert-subs" << "srt";
    }
    const double t0 = vdlParseTime(o.from), t1 = vdlParseTime(o.to);
    if (t0 >= 0 || t1 > 0) {
        a << "--download-sections" << QString("*%1-%2").arg(t0 >= 0 ? t0 : 0).arg(t1 > 0 ? QString::number(t1) : QString("inf"));
        a << "--force-keyframes-at-cuts";
    }
    if (!o.cookies.isEmpty()) a << "--cookies-from-browser" << o.cookies;
    a << (o.playlist ? "--yes-playlist" : "--no-playlist");
    a << "-P" << o.dir;
    a << "-o" << (o.playlist ? "%(playlist_title|Playlist)s/%(playlist_index|0)03d - %(title)s.%(ext)s" : "%(title)s.%(ext)s");
    a << "--progress-template"
      << "download:VDLP\t%(progress._percent_str)s\t%(progress._speed_str)s\t%(progress._eta_str)s\t%(info.playlist_index|1)s\t%(info.n_entries|1)s\t%(info.title)s";
    a << "--print" << "after_move:VDLF\t%(filepath)s";
    if (QFileInfo(ffmpeg).isAbsolute()) a << "--ffmpeg-location" << ffmpeg;
    a << url;
    return a;
}

static QString ytDlpDownloadUrl() {
    const QString base = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";
#if defined(Q_OS_WIN)
    return base + "yt-dlp.exe";
#elif defined(Q_OS_MAC)
    return base + "yt-dlp_macos";
#else
    return base + (QSysInfo::currentCpuArchitecture().contains("arm") ? "yt-dlp_linux_aarch64" : "yt-dlp_linux");
#endif
}

static QString defaultVideoDir() {
    QString d = QSettings().value("videodl/dir").toString();
    if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/Vidline";
    return d;
}

static QProcessEnvironment utf8Env() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");  // sonst kommen Umlaute in Titeln unter Windows kaputt an
    env.insert("PYTHONUTF8", "1");
    return env;
}

// ---------------------------------------------------------------- Fenster
VideoDownloadDialog::VideoDownloadDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(T("vdl_title"));
    setMinimumWidth(600);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);

    // Link
    auto* lrow = new QHBoxLayout;
    lrow->setSpacing(8);
    url_ = new QLineEdit;
    url_->setPlaceholderText(T("vdl_link_ph"));
    url_->setClearButtonEnabled(true);
    check_ = new QPushButton(T("vdl_check"));
    lrow->addWidget(url_, 1);
    lrow->addWidget(check_);
    v->addLayout(lrow);
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(700);
    connect(debounce_, &QTimer::timeout, this, [this] {
        const QString u = url_->text().trimmed();
        if (u.startsWith("http") && u.contains('.') && u != infoUrl_) analyze(u);
    });
    connect(url_, &QLineEdit::textChanged, this, [this] { debounce_->start(); });
    connect(url_, &QLineEdit::returnPressed, this, [this] { analyze(url_->text().trimmed()); });
    connect(check_, &QPushButton::clicked, this, [this] { analyze(url_->text().trimmed()); });

    // Vorschau: Bild, Titel, Kanal · Seite · Länge
    card_ = new QFrame;
    card_->setObjectName("card");
    auto* ch = new QHBoxLayout(card_);
    ch->setContentsMargins(12, 12, 12, 12);
    ch->setSpacing(14);
    thumb_ = new QLabel;
    thumb_->setFixedSize(176, 99);
    thumb_->setAlignment(Qt::AlignCenter);
    thumb_->setStyleSheet("background: rgba(127,127,127,0.18); border-radius: 8px;");
    ch->addWidget(thumb_);
    auto* tv = new QVBoxLayout;
    tv->setSpacing(4);
    title_ = new QLabel;
    title_->setObjectName("title");
    title_->setWordWrap(true);
    sub_ = new QLabel;
    sub_->setObjectName("hint");
    sub_->setWordWrap(true);
    tv->addWidget(title_);
    tv->addWidget(sub_);
    tv->addStretch();
    ch->addLayout(tv, 1);
    card_->hide();
    v->addWidget(card_);

    // Optionen
    opts_ = new QWidget;
    auto* form = new QFormLayout(opts_);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    what_ = new QComboBox;
    what_->addItem(T("vdl_what_both"), 0);
    what_->addItem(T("vdl_what_video"), 1);
    what_->addItem(T("vdl_what_audio"), 2);
    form->addRow(T("vdl_what"), what_);
    quality_ = new QComboBox;
    lblQuality_ = new QLabel(T("vdl_quality"));
    form->addRow(lblQuality_, quality_);
    format_ = new QComboBox;
    lblFormat_ = new QLabel(T("format"));
    form->addRow(lblFormat_, format_);
    compat_ = new QCheckBox(T("vdl_compat"));
    form->addRow(QString(), compat_);
    subs_ = new QComboBox;
    lblSubs_ = new QLabel(T("vdl_subs"));
    form->addRow(lblSubs_, subs_);
    // Abschnitt: nur ein Stück des Videos laden
    sectionRow_ = new QWidget;
    auto* sr = new QHBoxLayout(sectionRow_);
    sr->setContentsMargins(0, 0, 0, 0);
    sr->setSpacing(6);
    section_ = new QCheckBox(T("vdl_section"));
    from_ = new QLineEdit;
    to_ = new QLineEdit;
    for (QLineEdit* e : {from_, to_}) {
        e->setFixedWidth(80);
        e->setAlignment(Qt::AlignCenter);
    }
    from_->setPlaceholderText("0:00");
    to_->setPlaceholderText(T("vdl_end"));
    sr->addWidget(section_);
    sr->addWidget(from_);
    sr->addWidget(new QLabel("–"));
    sr->addWidget(to_);
    sr->addStretch();
    form->addRow(QString(), sectionRow_);
    playlist_ = new QCheckBox;
    form->addRow(QString(), playlist_);
    embed_ = new QCheckBox(T("vdl_embed"));
    form->addRow(QString(), embed_);
    cookies_ = new QComboBox;
    cookies_->addItem(T("vdl_login_none"), QString());
    for (const char* b : {"firefox", "edge", "chrome", "brave", "opera", "vivaldi", "safari"}) {
        QString n = QString::fromLatin1(b);
        n[0] = n[0].toUpper();
        cookies_->addItem(n, QString::fromLatin1(b));
    }
    cookies_->setToolTip(T("vdl_login_tip"));
    form->addRow(T("vdl_login"), cookies_);
    auto* frow = new QHBoxLayout;
    dir_ = defaultVideoDir();
    folder_ = new QLabel(QDir::toNativeSeparators(dir_));
    folder_->setObjectName("hint");
    folder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pick_ = new QPushButton(T("browse"));
    connect(pick_, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("folder"), dir_);
        if (d.isEmpty()) return;
        dir_ = d;
        QSettings().setValue("videodl/dir", d);
        folder_->setText(QDir::toNativeSeparators(d));
    });
    frow->addWidget(folder_, 1);
    frow->addWidget(pick_);
    form->addRow(T("folder"), frow);
    v->addWidget(opts_);

    // gemerkte Auswahl
    QSettings st;
    what_->setCurrentIndex(std::clamp(st.value("videodl/what", 0).toInt(), 0, 2));
    compat_->setChecked(st.value("videodl/compat", false).toBool());
    embed_->setChecked(st.value("videodl/embed", true).toBool());
    cookies_->setCurrentIndex(std::max(0, cookies_->findData(st.value("videodl/cookies", QString()))));
    fillFormats();
    fillQuality();
    connect(what_, &QComboBox::currentIndexChanged, this, [this] { fillFormats(); fillQuality(); updateOptionVisibility(); });
    connect(compat_, &QCheckBox::toggled, this, [this] { fillFormats(); fillQuality(); });
    connect(format_, &QComboBox::currentIndexChanged, this, [this] { fillQuality(); });
    connect(section_, &QCheckBox::toggled, this, [this](bool on) { from_->setEnabled(on); to_->setEnabled(on); });
    from_->setEnabled(false);
    to_->setEnabled(false);
    updateOptionVisibility();

    note_ = new QLabel(T("vdl_note"));
    note_->setObjectName("hint");
    note_->setWordWrap(true);
    v->addWidget(note_);
    status_ = new QLabel;
    status_->setWordWrap(true);
    v->addWidget(status_);

    // Warteschlange
    queueBox_ = new QFrame;
    auto* qv = new QVBoxLayout(queueBox_);
    qv->setContentsMargins(0, 0, 0, 0);
    qv->setSpacing(6);
    auto* qh = new QLabel(T("vdl_queue"));
    qh->setObjectName("title");
    qv->addWidget(qh);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(120);
    scroll->setMaximumHeight(260);
    auto* inner = new QWidget;
    inner->setObjectName("vdlqueue");
    inner->setStyleSheet("QWidget#vdlqueue { background: transparent; }");
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");
    queue_ = new QVBoxLayout(inner);
    queue_->setContentsMargins(0, 0, 0, 0);
    queue_->setSpacing(6);
    queue_->addStretch();
    scroll->setWidget(inner);
    qv->addWidget(scroll);
    queueBox_->hide();
    v->addWidget(queueBox_);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    close_ = new QPushButton(T("close"));
    start_ = new QPushButton(T("vdl_start"));
    start_->setObjectName("primary");
    start_->setEnabled(false);
    connect(close_, &QPushButton::clicked, this, &VideoDownloadDialog::reject);
    connect(start_, &QPushButton::clicked, this, &VideoDownloadDialog::download);
    btns->addWidget(close_);
    btns->addWidget(start_);
    v->addLayout(btns);
}

VideoDownloadDialog::~VideoDownloadDialog() {
    for (QProcess* p : {probe_, proc_})
        if (p) {
            p->disconnect();
            p->kill();
            p->waitForFinished(2000);
        }
    qDeleteAll(jobs_);
}

// Schließen, während geladen wird: Fenster verschwindet, die Downloads laufen weiter
void VideoDownloadDialog::closeEvent(QCloseEvent* e) {
    e->ignore();
    reject();
}
void VideoDownloadDialog::reject() { hide(); }

bool VideoDownloadDialog::analyzing() const { return settingUp_ || (probe_ && probe_->state() != QProcess::NotRunning); }
bool VideoDownloadDialog::busy() const {
    for (const Job* j : jobs_)
        if (j->state <= 1) return true;
    return false;
}
QString VideoDownloadDialog::status() const { return status_->text(); }
bool VideoDownloadDialog::hasThumb() const { return !thumb_->pixmap().isNull(); }
QStringList VideoDownloadDialog::finishedFiles() const {
    QStringList out;
    for (const Job* j : jobs_)
        if (j->state == 2) out << j->files;
    return out;
}
void VideoDownloadDialog::setStatus(const QString& s) { status_->setText(s); }

// yt-dlp einmalig von GitHub laden, danach höchstens alle 7 Tage aktualisieren (gleiche Datei wie das Musik-Plugin)
void VideoDownloadDialog::ensureYtDlp(std::function<void(bool, QString)> done) {
    const QString exe = ytDlpPath();
    if (QFileInfo(exe).size() > 1000000) {
        QSettings st;
        const qint64 last = st.value("music/ytdlpChecked", 0).toLongLong();
        if (QDateTime::currentSecsSinceEpoch() - last < 7 * 86400) return done(true, QString());
        st.setValue("music/ytdlpChecked", QDateTime::currentSecsSinceEpoch());
        setStatus(T("dl_update"));
        auto* p = new QProcess(this);
        connect(p, &QProcess::finished, this, [p, done] {
            p->deleteLater();
            done(true, QString());
        });
        p->start(exe, {"-U"});
        return;
    }
    if (!net_) net_ = new QNetworkAccessManager(this);
    QDir().mkpath(QFileInfo(exe).absolutePath());
    setStatus(T("dl_setup").arg("…"));
    QNetworkRequest req{QUrl(ytDlpDownloadUrl())};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Vidline");
    req.setTransferTimeout(30000);
    QNetworkReply* r = net_->get(req);
    connect(r, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        setStatus(T("dl_setup").arg(QString::number((total > 0 ? total : got) / 1048576.0, 'f', 0)));
    });
    connect(r, &QNetworkReply::finished, this, [r, exe, done] {
        r->deleteLater();
        const QByteArray data = r->readAll();
        if (r->error() != QNetworkReply::NoError || data.size() < 1000000) return done(false, r->errorString());
        QSaveFile f(exe);
        if (!f.open(QIODevice::WriteOnly)) return done(false, f.errorString());
        f.write(data);
        f.commit();
        QFile::setPermissions(exe, QFile::permissions(exe) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
        done(true, QString());
    });
}

void VideoDownloadDialog::analyze(const QString& url) {
    if (url.isEmpty()) return;
    if (!QUrl(url).isValid() || !url.contains('.')) {
        setStatus(T("vdl_bad_link"));
        return;
    }
    url_->setText(url);
    debounce_->stop();
    infoUrl_ = url;
    info_ = VdlInfo();
    card_->hide();
    start_->setEnabled(false);
    check_->setEnabled(false);
    settingUp_ = true;
    ensureYtDlp([this](bool ok, const QString& err) {
        settingUp_ = false;
        if (!ok) {
            check_->setEnabled(true);
            setStatus(T("dl_failed") + (err.isEmpty() ? QString() : "\n" + err));
            return;
        }
        runAnalyze();
    });
}

void VideoDownloadDialog::runAnalyze() {
    setStatus(T("vdl_checking"));
    if (probe_) {
        probe_->disconnect();
        probe_->kill();
        probe_->deleteLater();
    }
    probe_ = new QProcess(this);
    probe_->setProcessEnvironment(utf8Env());
    QStringList a = {"-J", "--flat-playlist", "--no-warnings", "--no-colors"};
    const QString ck = cookies_->currentData().toString();
    if (!ck.isEmpty()) a << "--cookies-from-browser" << ck;
    // Link mit Video UND Playlist (YouTube "watch?v=…&list=…"): erst nur das Video zeigen
    a << "--no-playlist" << infoUrl_;
    QProcess* p = probe_;
    connect(p, &QProcess::finished, this, [this, p](int code) {
        const QByteArray out = p->readAllStandardOutput();
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        check_->setEnabled(true);
        info_ = parseVdlInfo(out);
        if (code != 0 || !info_.ok) {
            QString msg = err.section('\n', -1).trimmed();
            if (msg.contains("Sign in", Qt::CaseInsensitive) || msg.contains("login", Qt::CaseInsensitive) || msg.contains("cookies", Qt::CaseInsensitive))
                msg += "\n" + T("vdl_need_login");
            setStatus(T("vdl_check_failed") + (msg.isEmpty() ? QString() : "\n" + msg));
            info_.ok = false;
            return;
        }
        showInfo();
    });
    p->start(ytDlpPath(), a);
}

void VideoDownloadDialog::showInfo() {
    title_->setText(info_.title.isEmpty() ? infoUrl_ : info_.title);
    QStringList parts;
    if (!info_.uploader.isEmpty()) parts << info_.uploader;
    if (!info_.site.isEmpty()) parts << info_.site;
    if (info_.playlist) parts << T("vdl_videos").arg(info_.count);
    else if (info_.duration > 0) parts << hms(info_.duration);
    sub_->setText(parts.join("  ·  "));
    thumb_->clear();
    if (!info_.thumb.isEmpty()) {
        if (!net_) net_ = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(info_.thumb)};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(15000);
        QNetworkReply* r = net_->get(req);
        connect(r, &QNetworkReply::finished, this, [this, r] {
            r->deleteLater();
            QPixmap px;
            if (r->error() == QNetworkReply::NoError && px.loadFromData(r->readAll()))
                thumb_->setPixmap(px.scaled(thumb_->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                                      .copy(0, 0, thumb_->width(), thumb_->height()));
        });
    }
    card_->show();
    // Playlist: ganze Liste oder (bei watch?v=…&list=) nur dieses Video
    const bool hasList = info_.playlist || QUrl(infoUrl_).query().contains("list=");
    playlist_->setVisible(hasList);
    playlist_->setText(info_.playlist ? T("vdl_whole_list_n").arg(info_.count) : T("vdl_whole_list"));
    playlist_->setChecked(info_.playlist);
    // Untertitel
    subs_->clear();
    subs_->addItem(T("vdl_subs_none"), QString());
    for (const QString& l : info_.subs) subs_->addItem(l, l);
    for (const QString& l : info_.autoSubs) subs_->addItem(l + " " + T("vdl_auto"), "auto:" + l);
    const QString lastSub = QSettings().value("videodl/sub").toString();
    if (!lastSub.isEmpty()) subs_->setCurrentIndex(std::max(0, subs_->findData(lastSub)));
    fillQuality();
    updateOptionVisibility();
    start_->setEnabled(true);
    setStatus(QString());
}

// Formate passend zur Auswahl "Video mit Ton / nur Video / nur Ton"
void VideoDownloadDialog::fillFormats() {
    const bool audio = what_->currentData().toInt() == 2;
    const QString key = audio ? "videodl/afmt" : "videodl/vfmt";
    const QString want = format_->count() && format_->property("audio").toBool() == audio ? format_->currentData().toString()
                                                                                          : QSettings().value(key).toString();
    QSignalBlocker b(format_);
    format_->clear();
    format_->setProperty("audio", audio);
    if (audio) {
        format_->addItem("MP3", "mp3");
        format_->addItem("M4A (AAC)", "m4a");
        format_->addItem("Opus", "opus");
        format_->addItem(T("vdl_lossless") + " FLAC", "flac");
        format_->addItem("WAV", "wav");
    } else if (compat_->isChecked()) {
        format_->addItem("MP4 (H.264)", "mp4");
    } else {
        format_->addItem("MP4", "mp4");
        format_->addItem("MKV", "mkv");
        format_->addItem("WebM", "webm");
    }
    format_->setCurrentIndex(std::max(0, format_->findData(want)));
}

void VideoDownloadDialog::fillQuality() {
    const int what = what_->currentData().toInt();
    const int wantH = quality_->count() ? quality_->currentData().toInt() : QSettings().value("videodl/height", 0).toInt();
    QSignalBlocker b(quality_);
    quality_->clear();
    if (what == 2) {
        const bool lossless = format_->currentData().toString() == "flac" || format_->currentData().toString() == "wav";
        quality_->addItem(T("vdl_best") + (info_.audioSize > 0 ? "  ·  ~" + vdlSize(info_.audioSize) : QString()), 0);
        if (!lossless)
            for (int k : {320, 256, 192, 128, 96}) quality_->addItem(QString("%1 kbit/s").arg(k), k);
        quality_->setCurrentIndex(std::max(0, quality_->findData(QSettings().value("videodl/kbps", 0).toInt())));
        return;
    }
    const bool compat = compat_->isChecked();
    auto label = [&](int h) {
        QString s = h >= 2160 ? QString("%1p (4K)").arg(h) : h >= 1440 ? QString("%1p (2K)").arg(h) : QString("%1p").arg(h);
        const int fps = info_.fpsAt.value(h);
        if (fps > 30) s = s.left(s.indexOf('p')) + QString("p%1").arg(fps) + s.mid(s.indexOf('p') + 1);
        qint64 size = compat ? info_.videoSizeH264.value(h) : info_.videoSize.value(h);
        if (size > 0 && what == 0) size += info_.audioSize;
        if (size > 0) s += "  ·  ~" + vdlSize(size);
        return s;
    };
    QList<int> hs;
    for (int h : info_.heights)
        if (!compat || info_.h264Heights.contains(h)) hs << h;
    if (hs.isEmpty() && (!info_.ok || info_.playlist)) hs = {2160, 1440, 1080, 720, 480, 360};  // Playlist: Formate noch unbekannt
    quality_->addItem(hs.isEmpty() || !info_.ok || info_.playlist ? T("vdl_best") : T("vdl_best") + "  ·  " + label(hs.first()), 0);
    for (int h : hs) quality_->addItem(info_.ok && !info_.playlist ? label(h) : QString("%1p").arg(h), h);
    quality_->setCurrentIndex(std::max(0, quality_->findData(wantH)));
}

void VideoDownloadDialog::updateOptionVisibility() {
    const bool audio = what_->currentData().toInt() == 2;
    compat_->setVisible(!audio);
    subs_->setVisible(!audio && subs_->count() > 1);
    lblSubs_->setVisible(!audio && subs_->count() > 1);
    lblQuality_->setText(audio ? T("vdl_bitrate") : T("vdl_quality"));
}

VdlOptions VideoDownloadDialog::currentOptions() const {
    VdlOptions o;
    o.what = what_->currentData().toInt();
    if (o.what == 2) {
        o.audioFmt = format_->currentData().toString();
        o.audioKbps = quality_->currentData().toInt();
    } else {
        o.height = quality_->currentData().toInt();
        o.container = format_->currentData().toString();
        o.compat = compat_->isChecked();
        const QString sub = subs_->currentData().toString();
        o.autoSub = sub.startsWith("auto:");
        o.subLang = o.autoSub ? sub.mid(5) : sub;
    }
    if (section_->isChecked()) {
        o.from = from_->text();
        o.to = to_->text();
    }
    o.playlist = playlist_->isVisible() && playlist_->isChecked();
    o.embed = embed_->isChecked();
    o.cookies = cookies_->currentData().toString();
    o.dir = dir_;
    return o;
}

void VideoDownloadDialog::saveOptions() const {
    QSettings st;
    st.setValue("videodl/what", what_->currentData());
    st.setValue("videodl/compat", compat_->isChecked());
    st.setValue("videodl/embed", embed_->isChecked());
    st.setValue("videodl/cookies", cookies_->currentData());
    if (what_->currentData().toInt() == 2) {
        st.setValue("videodl/afmt", format_->currentData());
        st.setValue("videodl/kbps", quality_->currentData());
    } else {
        st.setValue("videodl/vfmt", format_->currentData());
        st.setValue("videodl/height", quality_->currentData());
        st.setValue("videodl/sub", subs_->currentData());
    }
}

void VideoDownloadDialog::setOptionsForTest(const VdlOptions& o) {
    what_->setCurrentIndex(std::max(0, what_->findData(o.what)));
    compat_->setChecked(o.compat);
    fillFormats();
    format_->setCurrentIndex(std::max(0, format_->findData(o.what == 2 ? o.audioFmt : o.container)));
    fillQuality();
    quality_->setCurrentIndex(std::max(0, quality_->findData(o.what == 2 ? o.audioKbps : o.height)));
    section_->setChecked(!o.from.isEmpty() || !o.to.isEmpty());
    from_->setText(o.from);
    to_->setText(o.to);
    embed_->setChecked(o.embed);
    if (!o.dir.isEmpty()) {
        dir_ = o.dir;
        folder_->setText(QDir::toNativeSeparators(o.dir));
    }
}

void VideoDownloadDialog::download() {
    if (!info_.ok || infoUrl_.isEmpty()) {
        analyze(url_->text().trimmed());
        return;
    }
    VdlOptions o = currentOptions();
    if (section_->isChecked()) {
        const double t0 = vdlParseTime(o.from), t1 = vdlParseTime(o.to);
        if ((!o.from.trimmed().isEmpty() && t0 < 0) || (!o.to.trimmed().isEmpty() && t1 < 0) || (t1 > 0 && t0 >= t1)) {
            setStatus(T("vdl_bad_section"));
            return;
        }
    }
    saveOptions();
    auto* j = new Job;
    j->url = infoUrl_;
    j->title = info_.title.isEmpty() ? infoUrl_ : info_.title;
    j->opt = o;
    j->parts = o.what == 0 ? 2 : 1;
    jobs_ << j;
    addJobRow(j);
    // bereit für den nächsten Link
    url_->clear();
    infoUrl_.clear();
    info_ = VdlInfo();
    card_->hide();
    start_->setEnabled(false);
    setStatus(T("vdl_added"));
    pump();
}

void VideoDownloadDialog::addJobRow(Job* j) {
    queueBox_->show();
    j->row = new QFrame;
    j->row->setObjectName("card");
    auto* h = new QHBoxLayout(j->row);
    h->setContentsMargins(10, 8, 10, 8);
    h->setSpacing(10);
    auto* tv = new QVBoxLayout;
    tv->setSpacing(4);
    j->name = new QLabel(j->title);
    j->name->setObjectName("title");
    j->name->setMinimumWidth(10);
    j->state_ = new QLabel;
    j->state_->setObjectName("hint");
    j->bar = new QProgressBar;
    j->bar->setRange(0, 1000);
    j->bar->setTextVisible(false);
    j->bar->setFixedHeight(6);
    tv->addWidget(j->name);
    tv->addWidget(j->state_);
    tv->addWidget(j->bar);
    h->addLayout(tv, 1);
    j->cancel = new QPushButton(T("cancel"));
    j->open = new QPushButton(T("vdl_play"));
    j->open->setObjectName("primary");
    j->reveal = new QPushButton(T("vdl_show"));
    for (QPushButton* b : {j->open, j->reveal}) b->hide();
    connect(j->cancel, &QPushButton::clicked, this, [this, j] {
        if (j->state == 0) {
            j->state = 4;
            updateJobRow(j);
        } else if (j->state == 1 && proc_) {
            j->state = 4;
            proc_->kill();
        }
    });
    connect(j->open, &QPushButton::clicked, this, [this, j] {
        if (!j->files.isEmpty()) emit openFile(j->files.first());
    });
    connect(j->reveal, &QPushButton::clicked, this, [j] {
        if (j->files.isEmpty()) return;
#ifdef Q_OS_WIN
        QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(j->files.first())});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(j->files.first()).absolutePath()));
#endif
    });
    h->addWidget(j->cancel);
    h->addWidget(j->reveal);
    h->addWidget(j->open);
    queue_->insertWidget(queue_->count() - 1, j->row);
    updateJobRow(j);
}

void VideoDownloadDialog::updateJobRow(Job* j) {
    QString s;
    switch (j->state) {
    case 0: s = T("vdl_waiting"); break;
    case 1: s = j->line.isEmpty() ? T("vdl_starting") : j->line; break;
    case 2: {
        qint64 total = 0;
        for (const QString& f : j->files) total += QFileInfo(f).size();
        s = j->files.size() > 1 ? T("vdl_done_n").arg(j->files.size()).arg(vdlSize(total)) : T("vdl_done").arg(vdlSize(total));
        break;
    }
    case 3: s = T("dl_failed") + (j->error.isEmpty() ? QString() : "  ·  " + j->error); break;
    default: s = T("vdl_canceled");
    }
    j->state_->setText(s);
    j->state_->setToolTip(j->state == 3 ? j->error : QString());
    j->bar->setVisible(j->state <= 1);
    j->bar->setValue(int(std::clamp(j->pct, 0.0, 1.0) * 1000));
    j->cancel->setVisible(j->state <= 1);
    j->open->setVisible(j->state == 2 && !j->files.isEmpty());
    j->reveal->setVisible(j->state == 2 && !j->files.isEmpty());
}

void VideoDownloadDialog::pump() {
    if (running_) return;
    for (Job* j : jobs_)
        if (j->state == 0) {
            running_ = j;
            j->state = 1;
            updateJobRow(j);
            ensureYtDlp([this, j](bool ok, const QString& err) {
                if (!ok) {
                    j->state = 3;
                    j->error = err;
                    running_ = nullptr;
                    updateJobRow(j);
                    pump();
                    return;
                }
                if (j->state == 4) {  // inzwischen abgebrochen
                    running_ = nullptr;
                    updateJobRow(j);
                    pump();
                    return;
                }
                startJob(j);
            });
            return;
        }
}

void VideoDownloadDialog::startJob(Job* j) {
    QDir().mkpath(j->opt.dir);
    j->startedAt = QDateTime::currentMSecsSinceEpoch();
    proc_ = new QProcess(this);
    proc_->setProcessEnvironment(utf8Env());
    proc_->setProcessChannelMode(QProcess::MergedChannels);
    QProcess* p = proc_;
    connect(p, &QProcess::readyRead, this, [this, p, j] {
        while (p->canReadLine()) {
            const QString line = QString::fromUtf8(p->readLine()).trimmed();
            if (line.isEmpty()) continue;
            if (line.startsWith("VDLP\t")) {
                const QStringList f = line.split('\t');
                const double pct = f.value(1).remove('%').trimmed().toDouble() / 100.0;
                const int idx = std::max(1, f.value(4).toInt()), n = std::max(1, f.value(5).toInt());
                const double one = (std::max(0, j->part - 1) + pct) / std::max(1, j->parts);
                j->pct = ((idx - 1) + std::min(1.0, one)) / n;
                QStringList bits;
                bits << QString("%1 %").arg(qRound(j->pct * 100));
                const QString speed = f.value(2).trimmed(), eta = f.value(3).trimmed();
                if (!speed.isEmpty() && !speed.contains("Unknown") && speed != "NA") bits << speed;
                if (!eta.isEmpty() && !eta.contains("Unknown") && eta != "NA") bits << T("vdl_eta").arg(eta);
                if (n > 1) bits << T("vdl_item").arg(idx).arg(n);
                j->line = bits.join("  ·  ");
                if (n > 1 && !f.value(6).isEmpty()) j->name->setText(f.value(6));
                updateJobRow(j);
            } else if (line.startsWith("VDLF\t")) {
                const QString file = line.section('\t', 1);
                if (QFileInfo::exists(file)) j->files << file;
            } else if (line.startsWith("[download] Destination:")) {
                j->part = j->part % std::max(1, j->parts) + 1;
            } else if (line.startsWith("[Merger]") || line.startsWith("[ExtractAudio]") || line.startsWith("[VideoRemuxer]") ||
                       line.startsWith("[EmbedThumbnail]") || line.startsWith("[EmbedSubtitle]") || line.startsWith("[Metadata]")) {
                j->line = T("vdl_finishing");
                updateJobRow(j);
            } else if (line.startsWith("ERROR")) {
                j->error = line.mid(6).trimmed();
            }
        }
    });
    connect(p, &QProcess::finished, this, [this, p, j](int code) {
        p->deleteLater();
        if (proc_ == p) proc_ = nullptr;
        running_ = nullptr;
        if (j->state == 4 || (j->files.isEmpty())) {
            // Abgebrochen oder fehlgeschlagen: angefangene Teile wegräumen
            QDir d(j->opt.dir);
            for (const QFileInfo& fi : d.entryInfoList({"*.part", "*.ytdl", "*.part-Frag*", "*.temp.*"}, QDir::Files))
                if (fi.lastModified().toMSecsSinceEpoch() >= j->startedAt - 2000) QFile::remove(fi.absoluteFilePath());
            if (j->state != 4) {
                j->state = 3;
                if (j->error.isEmpty()) j->error = QString("yt-dlp: %1").arg(code);
                if (j->error.contains("Sign in", Qt::CaseInsensitive) || j->error.contains("login", Qt::CaseInsensitive))
                    j->error += "  ·  " + T("vdl_need_login");
            }
        } else {
            j->state = 2;
            j->pct = 1;
        }
        updateJobRow(j);
        pump();
    });
    p->start(ytDlpPath(), vdlArgs(j->opt, j->url, ffmpegPath()));
}
