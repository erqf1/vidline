#include "subtitles.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QVBoxLayout>
#include <cmath>
#include "i18n.h"
#include "icons.h"
#include "music.h"
#include "textrender.h"
#include "theme.h"

// ---------------------------------------------------------------- Dateien
QString subTime(double t, bool vtt) {
    const qint64 ms = std::max<qint64>(0, std::llround(t * 1000.0));
    return QString("%1:%2:%3%4%5")
        .arg(ms / 3600000, 2, 10, QChar('0'))
        .arg(ms / 60000 % 60, 2, 10, QChar('0'))
        .arg(ms / 1000 % 60, 2, 10, QChar('0'))
        .arg(vtt ? '.' : ',')
        .arg(ms % 1000, 3, 10, QChar('0'));
}

double parseSubTime(const QString& s) {
    static const QRegularExpression re(R"(^\s*(?:(\d+):)?(\d{1,2}):(\d{1,2})(?:[,.](\d{1,3}))?\s*$)");
    const auto m = re.match(s);
    if (!m.hasMatch()) return -1;
    const int h = m.captured(1).isEmpty() ? 0 : m.captured(1).toInt();
    const int mi = m.captured(2).toInt(), se = m.captured(3).toInt();
    if (mi > 59 || se > 59) return -1;
    QString frac = m.captured(4);
    while (frac.size() < 3) frac += '0';
    return h * 3600.0 + mi * 60.0 + se + (m.captured(4).isEmpty() ? 0 : frac.toInt() / 1000.0);
}

QString toSrt(const QList<SubCue>& cues) {
    QString out;
    int n = 0;
    for (const SubCue& c : cues) {
        if (c.text.trimmed().isEmpty()) continue;
        out += QString("%1\n%2 --> %3\n%4\n\n").arg(++n).arg(subTime(c.t0, false), subTime(c.t1, false), c.text.trimmed());
    }
    return out;
}

QString toVtt(const QList<SubCue>& cues) {
    QString out = "WEBVTT\n\n";
    for (const SubCue& c : cues) {
        if (c.text.trimmed().isEmpty()) continue;
        out += QString("%1 --> %2\n%3\n\n").arg(subTime(c.t0, true), subTime(c.t1, true), c.text.trimmed());
    }
    return out;
}

QList<SubCue> parseSubtitles(const QString& textIn) {
    QString text = textIn;
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    text.replace("\r\n", "\n").replace('\r', '\n');
    QList<SubCue> out;
    static const QRegularExpression tags("<[^>]*>");
    static const QRegularExpression arrow(R"(^\s*(\S+)\s*-->\s*(\S+))");
    for (const QString& block : text.split(QRegularExpression("\n\\s*\n"), Qt::SkipEmptyParts)) {
        const QStringList lines = block.split('\n');
        int at = -1;
        for (int i = 0; i < lines.size() && i < 3; ++i)
            if (lines[i].contains("-->")) { at = i; break; }
        if (at < 0) continue;  // WEBVTT-Kopf, NOTE, STYLE …
        const auto m = arrow.match(lines[at]);
        if (!m.hasMatch()) continue;
        SubCue c;
        c.t0 = parseSubTime(m.captured(1));
        c.t1 = parseSubTime(m.captured(2));
        if (c.t0 < 0 || c.t1 < 0) continue;
        QStringList body;
        for (int i = at + 1; i < lines.size(); ++i) body << QString(lines[i]).remove(tags).trimmed();
        c.text = body.join('\n').trimmed();
        out << c;
    }
    return out;
}

void normalizeCues(QList<SubCue>& cues, double total) {
    for (SubCue& c : cues) {
        c.text = c.text.trimmed();
        c.t0 = std::max(0.0, c.t0);
        if (c.t1 < c.t0) std::swap(c.t0, c.t1);
    }
    cues.erase(std::remove_if(cues.begin(), cues.end(), [total](const SubCue& c) {
                   return c.text.isEmpty() || c.t1 - c.t0 < 0.01 || (total > 0 && c.t0 >= total - 0.01);
               }), cues.end());
    std::stable_sort(cues.begin(), cues.end(), [](const SubCue& a, const SubCue& b) { return a.t0 < b.t0; });
    for (int i = 0; i < cues.size(); ++i) {
        if (total > 0) cues[i].t1 = std::min(cues[i].t1, total);
        if (i > 0 && cues[i].t0 < cues[i - 1].t1) {
            // Überschneidung: der vorige endet, wo der nächste beginnt (bleibt er dann zu kurz, rückt der nächste)
            if (cues[i].t0 - cues[i - 1].t0 >= 0.3) cues[i - 1].t1 = cues[i].t0;
            else cues[i].t0 = cues[i - 1].t1;
        }
    }
    cues.erase(std::remove_if(cues.begin(), cues.end(), [](const SubCue& c) { return c.t1 - c.t0 < 0.05; }), cues.end());
}

int cueAt(const QList<SubCue>& cues, double t) {
    for (int i = 0; i < cues.size(); ++i)
        if (t >= cues[i].t0 && t < cues[i].t1) return i;
    return -1;
}

QList<SubCue> parseWhisperJson(const QByteArray& json, QString* lang) {
    QList<SubCue> out;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (lang) *lang = root.value("result").toObject().value("language").toString();
    static const QRegularExpression noise(R"(^\s*[\[\(][^\]\)]*[\]\)]\s*$)");  // [BLANK_AUDIO], (Musik) …
    for (const QJsonValue& v : root.value("transcription").toArray()) {
        const QJsonObject o = v.toObject();
        const QJsonObject off = o.value("offsets").toObject();
        SubCue c;
        if (off.contains("from")) {
            c.t0 = off.value("from").toDouble() / 1000.0;
            c.t1 = off.value("to").toDouble() / 1000.0;
        } else {
            const QJsonObject ts = o.value("timestamps").toObject();
            c.t0 = parseSubTime(ts.value("from").toString());
            c.t1 = parseSubTime(ts.value("to").toString());
        }
        c.text = o.value("text").toString().trimmed();
        if (c.text.isEmpty() || noise.match(c.text).hasMatch() || c.t0 < 0 || c.t1 < 0) continue;
        out << c;
    }
    return out;
}

// ---------------------------------------------------------------- Darstellung
static QStringList wrapLines(const QString& text, const QFontMetricsF& fm, double maxW) {
    QStringList out;
    for (const QString& para : text.split('\n')) {
        QString line;
        for (const QString& word : para.split(' ', Qt::SkipEmptyParts)) {
            const QString cand = line.isEmpty() ? word : line + ' ' + word;
            if (!line.isEmpty() && fm.horizontalAdvance(cand) > maxW) {
                out << line;
                line = word;
            } else {
                line = cand;
            }
        }
        out << line;
    }
    return out;
}

QImage renderSubtitleFrame(const QString& text, const SubStyle& st, QSize frame) {
    frame = frame.expandedTo(QSize(2, 2));
    QImage img(frame, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if (text.trimmed().isEmpty()) return img;
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const double unit = std::min(frame.width(), frame.height()) / 1080.0;
    QFont f(st.font.isEmpty() ? defaultTextFont() : st.font);
    f.setPixelSize(std::max(6, int(std::lround(st.size * unit))));
    f.setBold(st.bold);
    const QFontMetricsF fm(f);
    const QStringList lines = wrapLines(text.trimmed(), fm, frame.width() * 0.86);
    const double lh = fm.height() * 1.08, blockH = lh * lines.size();
    const double margin = std::clamp(st.margin, 0.0, 0.45) * frame.height();
    double y = st.pos == 2 ? margin : st.pos == 1 ? (frame.height() - blockH) / 2 : frame.height() - margin - blockH;
    const QColor fg = QColor::fromRgba(st.color), edge = QColor::fromRgba(st.edgeColor), box = QColor::fromRgba(st.boxColor);
    for (const QString& line : lines) {
        const double w = fm.horizontalAdvance(line);
        const double x = (frame.width() - w) / 2;
        const double base = y + (lh - fm.height()) / 2 + fm.ascent();
        QPainterPath path;
        path.addText(QPointF(x, base), f, line);
        if (st.look == 1) {
            const double padX = f.pixelSize() * 0.35, padY = f.pixelSize() * 0.08;
            p.setPen(Qt::NoPen);
            p.setBrush(box);
            p.drawRoundedRect(QRectF(x - padX, y - padY, w + 2 * padX, lh + 2 * padY), f.pixelSize() * 0.18, f.pixelSize() * 0.18);
        } else if (st.look == 2) {
            const double off = std::max(1.5, f.pixelSize() * 0.06);
            QColor sh = edge;
            sh.setAlphaF(sh.alphaF() * 0.75);
            p.setPen(Qt::NoPen);
            p.setBrush(sh);
            p.drawPath(path.translated(off, off));
        } else {
            QPen pen(edge, std::max(2.0, f.pixelSize() * 0.12), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.strokePath(path, pen);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(fg);
        p.drawPath(path);
        y += lh;
    }
    return img;
}

QString writeSubtitleBurn(const QList<SubCue>& cuesIn, const SubStyle& st, QSize frame, double total, const QString& dir) {
    QList<SubCue> cues = cuesIn;
    normalizeCues(cues, total);
    QDir d(dir);
    d.mkpath(".");
    QImage blank(frame, QImage::Format_ARGB32_Premultiplied);
    blank.fill(Qt::transparent);
    blank.save(d.filePath("blank.png"));
    auto n = [](double v) { return QString::number(std::max(0.001, v), 'f', 3); };
    QString list = "ffconcat version 1.0\n";
    double t = 0;
    for (int i = 0; i < cues.size(); ++i) {
        const SubCue& c = cues[i];
        if (c.t0 > t + 0.0005) list += QString("file 'blank.png'\nduration %1\n").arg(n(c.t0 - t));
        const QString name = QString("c%1.png").arg(i + 1, 4, 10, QChar('0'));
        renderSubtitleFrame(c.text, st, frame).save(d.filePath(name));
        list += QString("file '%1'\nduration %2\n").arg(name, n(c.t1 - std::max(t, c.t0)));
        t = c.t1;
    }
    list += QString("file 'blank.png'\nduration %1\nfile 'blank.png'\n").arg(n(std::max(1.0, total - t + 1.0)));
    QFile f(d.filePath("subs.ffconcat"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return QString();
    f.write(list.toUtf8());
    return f.fileName();
}

// ---------------------------------------------------------------- whisper.cpp
const QList<WhisperModel>& whisperModels() {
    static const QList<WhisperModel> m = {
        {"tiny", "wm_tiny", 75},
        {"base", "wm_base", 142},
        {"small", "wm_small", 466},
        {"large-v3-turbo-q5_0", "wm_turbo", 547},
        {"medium", "wm_medium", 1460},
    };
    return m;
}

QString whisperDir() { return musicDataDir() + "/plugins/whisper"; }

QString whisperExe() {
    const QString env = qEnvironmentVariable("VIDLINE_WHISPER_BIN");
    if (!env.isEmpty()) return env;
#ifdef Q_OS_WIN
    return whisperDir() + "/bin/whisper-cli.exe";
#else
    return whisperDir() + "/bin/whisper-cli";
#endif
}

QString whisperModelPath(const QString& id) { return whisperDir() + "/models/ggml-" + id + ".bin"; }

bool whisperModelReady(const QString& id) {
    for (const WhisperModel& m : whisperModels())
        if (id == m.id) return QFileInfo(whisperModelPath(id)).size() > qint64(m.mb) * 900000;
    return false;
}

QString whisperProgramAsset() {
#if defined(Q_OS_WIN)
    return QSysInfo::currentCpuArchitecture() == "x86_64" ? "whisper-windows-x64.zip" : QString();
#elif defined(Q_OS_MAC)
    return QSysInfo::currentCpuArchitecture().contains("arm") ? "whisper-macos-apple-silicon.tar.gz" : "whisper-macos-intel.tar.gz";
#else
    return QSysInfo::currentCpuArchitecture() == "x86_64" ? "whisper-linux-x86_64.tar.gz" : QString();
#endif
}

// whisper.cpp hat keine fertigen Programme für Linux und macOS. Deshalb baut der Release-Workflow von Vidline
// whisper-cli aus dem offiziellen Quellcode (fester Tag) und legt es für alle Systeme als eigene Datei ins Release.
QStringList whisperProgramUrls() {
    const QString env = qEnvironmentVariable("VIDLINE_WHISPER_URL");
    if (!env.isEmpty()) return {env};
    const QString asset = whisperProgramAsset();
    if (asset.isEmpty()) return {};
    const QString base = "https://github.com/erqf1/vidline/releases/";
    return {base + "download/v" APP_VERSION "/" + asset, base + "latest/download/" + asset};
}

QString whisperModelUrl(const QString& id) {
    return "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-" + id + ".bin";
}

QStringList whisperArgs(const QString& model, const QString& wav, const QString& lang, bool translate,
                        const QString& outBase, int threads) {
    QStringList a = {"-m", model, "-f", wav, "-l", lang.isEmpty() ? QString("auto") : lang, "-oj", "-of", outBase,
                     "-pp", "-t", QString::number(std::max(1, threads)), "-ml", "42", "-sow"};
    if (translate) a << "-tr";
    return a;
}

QList<QPair<QString, QString>> whisperLanguages() {
    return {{"auto", T("sub_lang_auto")}, {"de", "Deutsch"}, {"en", "English"}, {"es", "Español"}, {"fr", "Français"},
            {"it", "Italiano"}, {"pt", "Português"}, {"nl", "Nederlands"}, {"pl", "Polski"}, {"tr", "Türkçe"},
            {"ru", "Русский"}, {"uk", "Українська"}, {"ja", "日本語"}, {"zh", "中文"}, {"ko", "한국어"},
            {"ar", "العربية"}, {"sv", "Svenska"}, {"da", "Dansk"}, {"no", "Norsk"}, {"fi", "Suomi"},
            {"cs", "Čeština"}, {"el", "Ελληνικά"}, {"hu", "Magyar"}, {"ro", "Română"}, {"hi", "हिन्दी"}};
}

WhisperJob::WhisperJob(const Project& pr, QString model, QString lang, bool translate, QObject* parent)
    : QObject(parent), pr_(pr), model_(std::move(model)), wantLang_(std::move(lang)), translate_(translate) {}

WhisperJob::~WhisperJob() {
    cancelled_ = true;
    if (reply_) reply_->abort();
    if (proc_) {
        proc_->disconnect(this);
        proc_->kill();
        proc_->waitForFinished(3000);
    }
}

void WhisperJob::start() {
    running_ = true;
    cancelled_ = false;
    cues_.clear();
    tmp_ = std::make_unique<QTemporaryDir>();
    ensureProgram();
}

void WhisperJob::cancel() {
    cancelled_ = true;
    if (reply_) reply_->abort();
    if (proc_) proc_->kill();
}

void WhisperJob::fail(const QString& err) {
    running_ = false;
    emit finished(false, cancelled_ ? T("cancelled") : err);
}

// Datei laden und dabei direkt auf die Platte schreiben (Modelle sind bis 1,5 GB groß); erst am Ende umbenennen
void WhisperJob::fetch(const QStringList& urls, const QString& dest, qint64 minBytes, const QString& statusKey,
                       std::function<void()> next) {
    if (urls.isEmpty()) return fail(T("sub_no_program"));
    if (!net_) net_ = new QNetworkAccessManager(this);
    QDir().mkpath(QFileInfo(dest).absolutePath());
    auto file = std::make_shared<QFile>(dest + ".part");
    if (!file->open(QIODevice::WriteOnly)) return fail(file->errorString());
    QNetworkRequest req{QUrl(urls.first())};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Vidline");
    req.setTransferTimeout(60000);
    reply_ = net_->get(req);
    emit progress(T(statusKey.toLatin1().constData()).arg("0"), 0);
    connect(reply_, &QNetworkReply::readyRead, this, [this, file] { file->write(reply_->readAll()); });
    connect(reply_, &QNetworkReply::downloadProgress, this, [this, statusKey](qint64 got, qint64 total) {
        const QString mb = QString::number(got / 1048576.0, 'f', 0) +
                           (total > 0 ? " / " + QString::number(total / 1048576.0, 'f', 0) : QString()) + " MB";
        emit progress(T(statusKey.toLatin1().constData()).arg(mb), total > 0 ? double(got) / total : -1);
    });
    connect(reply_, &QNetworkReply::finished, this, [this, file, urls, dest, minBytes, statusKey, next] {
        QNetworkReply* r = reply_;
        reply_ = nullptr;
        r->deleteLater();
        file->write(r->readAll());
        file->close();
        const bool ok = r->error() == QNetworkReply::NoError && file->size() >= minBytes;
        if (!ok) {
            file->remove();
            if (cancelled_) return fail(QString());
            if (urls.size() > 1) return fetch(urls.mid(1), dest, minBytes, statusKey, next);
            return fail(T("dl_failed") + "\n" + r->errorString());
        }
        QFile::remove(dest);
        if (!file->rename(dest)) return fail(file->errorString());
        next();
    });
}

void WhisperJob::ensureProgram() {
    if (QFileInfo(whisperExe()).isExecutable()) return ensureModel();
    if (!qEnvironmentVariable("VIDLINE_WHISPER_BIN").isEmpty()) return fail(T("sub_no_program"));
    const QString asset = whisperProgramAsset();
    if (asset.isEmpty()) return fail(T("sub_no_program"));
    const QString archive = whisperDir() + "/" + asset;
    fetch(whisperProgramUrls(), archive, 200000, "sub_get_program", [this, archive] {
        // Entpacken mit tar (gibt es auf Windows 10/11, macOS und Linux; liest auch .zip)
        const QString bin = whisperDir() + "/bin";
        QDir(bin).removeRecursively();
        QDir().mkpath(bin);
        auto* p = new QProcess(this);
        proc_ = p;
        connect(p, &QProcess::finished, this, [this, p, archive, bin](int code, QProcess::ExitStatus st) {
            p->deleteLater();
            proc_ = nullptr;
            QFile::remove(archive);
            // whisper-cli kann im Archiv auch in einem Unterordner liegen
            QDirIterator it(bin, {"whisper-cli", "whisper-cli.exe"}, QDir::Files, QDirIterator::Subdirectories);
            const QString found = it.hasNext() ? it.next() : QString();
            if (st != QProcess::NormalExit || code != 0 || found.isEmpty()) return fail(T("sub_no_program"));
            if (QFileInfo(found).absolutePath() != QFileInfo(whisperExe()).absolutePath()) {
                // alles aus dem Unterordner nach bin/ holen (Programm + Bibliotheken)
                QDir sub = QFileInfo(found).dir();
                for (const QString& f : sub.entryList(QDir::Files)) QFile::rename(sub.filePath(f), bin + "/" + f);
            }
            QFile::setPermissions(whisperExe(), QFile::permissions(whisperExe()) | QFile::ExeOwner | QFile::ExeGroup |
                                                    QFile::ExeOther | QFile::ReadOwner);
            ensureModel();
        });
        p->start("tar", {"-xf", archive, "-C", bin});
    });
}

void WhisperJob::ensureModel() {
    if (cancelled_) return fail(QString());
    if (whisperModelReady(model_)) return extractAudio();
    int mb = 50;
    for (const WhisperModel& m : whisperModels())
        if (model_ == m.id) mb = m.mb;
    fetch({whisperModelUrl(model_)}, whisperModelPath(model_), qint64(mb) * 900000, "sub_get_model", [this] { extractAudio(); });
}

void WhisperJob::extractAudio() {
    if (cancelled_) return fail(QString());
    emit progress(T("sub_prepare"), -1);
    ExportOptions o;
    o.format = "wav";
    o.sampleRate = 16000;
    o.mono = true;
    const QString wav = tmp_->filePath("audio.wav");
    QStringList args = buildExport(pr_, o, wav);
    auto* p = new QProcess(this);
    proc_ = p;
    auto err = std::make_shared<QString>();
    connect(p, &QProcess::readyReadStandardError, this, [p, err] { *err += QString::fromUtf8(p->readAllStandardError()); });
    connect(p, &QProcess::finished, this, [this, p, wav, err](int code, QProcess::ExitStatus st) {
        p->deleteLater();
        proc_ = nullptr;
        if (cancelled_) return fail(QString());
        if (st != QProcess::NormalExit || code != 0 || QFileInfo(wav).size() < 1000) return fail(err->right(800));
        transcribe();
    });
    p->start(ffmpegPath(), args);
}

void WhisperJob::transcribe() {
    emit progress(T("sub_recognize").arg(0), 0);
    const QString outBase = tmp_->filePath("result");
    const int threads = std::clamp(QThread::idealThreadCount(), 1, 8);
    auto* p = new QProcess(this);
    proc_ = p;
    auto log = std::make_shared<QString>();
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::readyRead, this, [this, p, log] {
        const QString chunk = QString::fromUtf8(p->readAll());
        *log += chunk;
        if (log->size() > 20000) *log = log->right(10000);
        static const QRegularExpression re(R"(progress\s*=\s*(\d+)%)");
        auto it = re.globalMatch(chunk);
        int pct = -1;
        while (it.hasNext()) pct = it.next().captured(1).toInt();
        if (pct >= 0) emit progress(T("sub_recognize").arg(pct), pct / 100.0);
    });
    connect(p, &QProcess::finished, this, [this, p, outBase, log](int code, QProcess::ExitStatus st) {
        p->deleteLater();
        proc_ = nullptr;
        if (cancelled_) return fail(QString());
        QFile f(outBase + ".json");
        if (st != QProcess::NormalExit || code != 0 || !f.open(QIODevice::ReadOnly)) return fail(log->right(800));
        cues_ = parseWhisperJson(f.readAll(), &lang_);
        normalizeCues(cues_, pr_.total());
        running_ = false;
        emit finished(true, QString());
    });
    p->start(whisperExe(), whisperArgs(whisperModelPath(model_), tmp_->filePath("audio.wav"), wantLang_, translate_,
                                        outBase, threads));
}

// ---------------------------------------------------------------- Fenster
SubtitleDialog::SubtitleDialog(const Project& pr, QWidget* parent) : QDialog(parent), pr_(pr) {
    setWindowTitle(T("sub_auto"));
    setMinimumWidth(460);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    auto* head = new QHBoxLayout;
    auto* icon = new QLabel;
    icon->setPixmap(makeIcon(Ic::Subtitles, currentTheme().accent).pixmap(30, 30));
    head->addWidget(icon);
    auto* title = new QLabel(T("sub_auto"));
    title->setObjectName("title");
    head->addWidget(title, 1);
    v->addLayout(head);
    auto* hint = new QLabel(T("sub_auto_hint"));
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    v->addWidget(hint);
    auto* form = new QFormLayout;
    form->setSpacing(10);
    model_ = new QComboBox;
    fillModels();
    form->addRow(T("sub_model"), model_);
    lang_ = new QComboBox;
    for (const auto& l : whisperLanguages()) lang_->addItem(l.second, l.first);
    const QString lastLang = QSettings().value("subs/lang", "auto").toString();
    lang_->setCurrentIndex(std::max(0, lang_->findData(lastLang)));
    form->addRow(T("sub_language"), lang_);
    v->addLayout(form);
    translate_ = new QCheckBox(T("sub_translate"));
    v->addWidget(translate_);
    replace_ = new QCheckBox(T("sub_replace"));
    replace_->setChecked(true);
    replace_->setVisible(!pr.subs.isEmpty());
    v->addWidget(replace_);
    status_ = new QLabel;
    status_->setObjectName("hint");
    status_->setWordWrap(true);
    v->addWidget(status_);
    bar_ = new QProgressBar;
    bar_->setRange(0, 1000);
    bar_->setTextVisible(false);
    bar_->setFixedHeight(8);
    bar_->hide();
    v->addWidget(bar_);
    auto* row = new QHBoxLayout;
    row->addStretch();
    cancel_ = new QPushButton(T("cancel"));
    go_ = new QPushButton(T("sub_start"));
    go_->setObjectName("primary");
    go_->setDefault(true);
    row->addWidget(cancel_);
    row->addWidget(go_);
    v->addLayout(row);
    connect(cancel_, &QPushButton::clicked, this, &SubtitleDialog::reject);
    connect(go_, &QPushButton::clicked, this, &SubtitleDialog::start);
}

void SubtitleDialog::fillModels() {
    const QString last = QSettings().value("subs/model", "base").toString();
    model_->clear();
    for (const WhisperModel& m : whisperModels()) {
        const QString size = m.mb >= 1000 ? QString("%1 GB").arg(m.mb / 1000.0, 0, 'f', 1) : QString("%1 MB").arg(m.mb);
        model_->addItem(QString("%1 (%2)%3").arg(T(m.key), size, whisperModelReady(m.id) ? "  ·  " + T("sub_ready") : QString()),
                        QString(m.id));
    }
    model_->setCurrentIndex(std::max(0, model_->findData(last)));
}

bool SubtitleDialog::replaceExisting() const { return replace_->isChecked() || pr_.subs.isEmpty(); }

QString SubtitleDialog::status() const { return status_->text(); }

void SubtitleDialog::startForTest(const QString& model, const QString& lang) {
    model_->setCurrentIndex(std::max(0, model_->findData(model)));
    lang_->setCurrentIndex(std::max(0, lang_->findData(lang)));
    start();
}

void SubtitleDialog::start() {
    if (job_) return;
    QSettings st;
    st.setValue("subs/model", model_->currentData());
    st.setValue("subs/lang", lang_->currentData());
    for (QWidget* w : std::initializer_list<QWidget*>{model_, lang_, translate_, replace_, go_}) w->setEnabled(false);
    bar_->show();
    bar_->setValue(0);
    job_ = new WhisperJob(pr_, model_->currentData().toString(), lang_->currentData().toString(), translate_->isChecked(), this);
    connect(job_, &WhisperJob::progress, this, [this](const QString& s, double f) {
        status_->setText(s);
        if (f < 0) bar_->setRange(0, 0);
        else {
            bar_->setRange(0, 1000);
            bar_->setValue(int(f * 1000));
        }
    });
    connect(job_, &WhisperJob::finished, this, [this](bool ok, const QString& err) {
        WhisperJob* j = job_;
        job_ = nullptr;
        j->deleteLater();
        if (ok && !j->cues().isEmpty()) {
            cues_ = j->cues();
            status_->setText(T("sub_found").arg(cues_.size()));
            accept();
            return;
        }
        bar_->hide();
        status_->setText(ok ? T("sub_none") : err);
        for (QWidget* w : std::initializer_list<QWidget*>{model_, lang_, translate_, replace_, go_}) w->setEnabled(true);
        fillModels();
    });
    job_->start();
}

void SubtitleDialog::reject() {
    if (job_) {
        job_->cancel();
        return;  // finished() meldet "abgebrochen", dann kann man schließen
    }
    QDialog::reject();
}
