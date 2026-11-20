#include "presets.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>
#include "i18n.h"
#include "subtitles.h"
#include "textrender.h"

static int evenI(double v) { return std::max(2, int(std::lround(v / 2.0)) * 2); }

QString ExportPreset::title() const { return builtin && key ? T(key) : name; }

QList<ExportPreset> builtinPresets() {
    QList<ExportPreset> l;
    auto add = [&l](const char* id, const char* key, int w, int h, int fit) -> ExportPreset& {
        ExportPreset p;
        p.id = id;
        p.key = key;
        p.w = w;
        p.h = h;
        p.fit = fit;
        l << p;
        return l.last();
    };
    add("tiktok", "pr_tiktok", 1080, 1920, 0).audioKbps = 192;
    add("yt1080", "pr_yt1080", 1920, 1080, 0).quality = 0;
    add("yt4k", "pr_yt4k", 3840, 2160, 0).quality = 0;
    {
        ExportPreset& p = add("discord10", "pr_discord10", 0, 0, 0);
        p.maxMB = 10;
        p.audioKbps = 96;
        p.maxShort = 720;
        p.maxFps = 30;
    }
    {
        ExportPreset& p = add("discord50", "pr_discord50", 0, 0, 0);
        p.maxMB = 50;
        p.audioKbps = 128;
        p.maxShort = 1080;
    }
    add("insta", "pr_insta", 1080, 1080, 1).maxFps = 30;
    {
        ExportPreset& p = add("gif", "pr_gif", 0, 0, 0);
        p.format = "gif";
        p.gifWidth = 480;
        p.gifFps = 12;
    }
    return l;
}

QString presetToJson(const ExportPreset& p) {
    QJsonObject o{{"name", p.name}, {"w", p.w}, {"h", p.h}, {"fit", p.fit}, {"maxFps", p.maxFps},
                  {"format", p.format}, {"quality", p.quality}, {"maxMB", p.maxMB}, {"audioKbps", p.audioKbps},
                  {"maxShort", p.maxShort}, {"gifWidth", p.gifWidth}, {"gifFps", p.gifFps}};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

ExportPreset presetFromJson(const QString& json) {
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    ExportPreset p;
    p.builtin = false;
    p.name = o.value("name").toString();
    p.id = "custom:" + p.name;
    p.w = std::clamp(o.value("w").toInt(), 0, 7680);
    p.h = std::clamp(o.value("h").toInt(), 0, 7680);
    p.fit = std::clamp(o.value("fit").toInt(), 0, 2);
    p.maxFps = std::clamp(o.value("maxFps").toDouble(60), 1.0, 240.0);
    p.format = o.value("format").toString("mp4");
    if (!QStringList{"mp4", "mov", "mkv", "gif", "mp3", "wav", "m4a"}.contains(p.format)) p.format = "mp4";
    p.quality = std::clamp(o.value("quality").toInt(1), 0, 2);
    p.maxMB = std::max(0.0, o.value("maxMB").toDouble());
    p.audioKbps = std::clamp(o.value("audioKbps").toInt(), 0, 512);
    p.maxShort = std::max(0, o.value("maxShort").toInt());
    p.gifWidth = std::clamp(o.value("gifWidth").toInt(640), 16, 3840);
    p.gifFps = std::clamp(o.value("gifFps").toDouble(15), 1.0, 50.0);
    return p;
}

QList<ExportPreset> customPresets() {
    QList<ExportPreset> l;
    for (const QString& j : QSettings().value("export/presets").toStringList()) {
        ExportPreset p = presetFromJson(j);
        if (!p.name.isEmpty()) l << p;
    }
    return l;
}

QList<ExportPreset> allPresets() { return builtinPresets() + customPresets(); }

const ExportPreset* findPreset(const QList<ExportPreset>& list, const QString& id) {
    for (const ExportPreset& p : list)
        if (p.id == id) return &p;
    return nullptr;
}

void saveCustomPreset(const ExportPreset& in) {
    ExportPreset p = in;
    p.builtin = false;
    QStringList out;
    for (const ExportPreset& c : customPresets())
        if (c.name != p.name) out << presetToJson(c);
    out << presetToJson(p);
    QSettings().setValue("export/presets", out);
}

void removeCustomPreset(const QString& name) {
    QStringList out;
    for (const ExportPreset& c : customPresets())
        if (c.name != name) out << presetToJson(c);
    QSettings().setValue("export/presets", out);
}

int sizeTargetVideoKbps(double maxBytes, double duration, int audioKbps) {
    if (duration <= 0.05 || maxBytes <= 0) return 0;
    // 5 % Reserve für Container und Schwankungen der Bitrate
    const double total = maxBytes * 8.0 * 0.95 / duration / 1000.0;
    return std::max(1, int(std::floor(total - audioKbps)));
}

QSize compositionSize(int cw, int ch, int tw, int th, int fit) {
    if (cw <= 0 || ch <= 0 || tw <= 0 || th <= 0) return {std::max(2, tw), std::max(2, th)};
    if (std::abs(double(cw) / ch - double(tw) / th) < 0.01) return {tw, th};
    const double k = fit == 1 ? std::max(double(tw) / cw, double(th) / ch) : std::min(double(tw) / cw, double(th) / ch);
    return {evenI(cw * k), evenI(ch * k)};
}

ExportPlan planPreset(const ExportPreset& p, int cw, int ch, double srcFps, double duration, bool hasAudio) {
    ExportPlan plan;
    ExportOptions& o = plan.o;
    cw = std::max(2, cw);
    ch = std::max(2, ch);
    o.format = p.format;
    o.quality = p.quality;
    o.fit = p.fit;
    o.fps = std::min(srcFps > 1 ? srcFps : 30.0, p.maxFps);
    o.audioKbps = p.audioKbps;
    o.gifWidth = p.gifWidth;
    o.gifFps = p.gifFps;
    int tw = p.w > 0 ? p.w : cw, th = p.h > 0 ? p.h : ch;
    if (p.w <= 0 && p.maxShort > 0 && std::min(tw, th) > p.maxShort) {
        const double k = double(p.maxShort) / std::min(tw, th);
        tw = evenI(tw * k);
        th = evenI(th * k);
    }
    if (p.maxMB > 0 && duration > 0 && !isAudioFormat(p.format) && p.format != "gif") {
        plan.maxBytes = p.maxMB * 1e6;
        plan.twoPass = true;
        const int audio = hasAudio ? std::max(32, p.audioKbps > 0 ? p.audioKbps : 128) : 0;
        o.audioKbps = audio;
        int vk = sizeTargetVideoKbps(plan.maxBytes, duration, audio);
        // Lieber kleiner als matschig: die kurze Seite so wählen, dass genug Bitrate pro Bildpunkt bleibt
        const int shortNow = std::min(tw, th);
        int pick = 0;
        for (int s : {shortNow, 1080, 720, 540, 480, 360, 240}) {
            if (s > shortNow) continue;
            const double k = double(s) / shortNow;
            const double px = double(evenI(tw * k)) * evenI(th * k) * o.fps;
            if (vk * 1000.0 / px >= 0.045) { pick = s; break; }
        }
        if (pick == 0) pick = std::min(shortNow, 240);
        if (pick != shortNow) {
            const double k = double(pick) / shortNow;
            tw = evenI(tw * k);
            th = evenI(th * k);
        }
        if (vk < 100) {
            plan.tooLong = true;
            vk = std::max(vk, 40);
        }
        o.videoKbps = vk;
    }
    const QSize comp = compositionSize(cw, ch, tw, th, p.fit);
    o.width = comp.width();
    o.height = comp.height();
    o.outW = (comp.width() != tw || comp.height() != th) ? tw : 0;
    o.outH = o.outW ? th : 0;
    return plan;
}

ExportJob prepareExport(const Project& pr, ExportOptions eo, const QString& out, bool twoPass, int subMode) {
    ExportJob job;
    job.tmp = std::make_shared<QTemporaryDir>();
    Project ep = pr;
    // Texte und Zeichnungen in Export-Auflösung als Bilder (sieht genauso aus wie in der Vorschau)
    for (Item& it : ep.items) {
        if (it.kind != Item::Text && !it.graphic()) continue;
        const QString png = job.tmp->filePath(QString("text%1.png").arg(it.id));
        renderItemImage(it, QSize(std::max(2, int(it.w * eo.width)), std::max(2, int(it.h * eo.height))), eo.height).save(png);
        it.path = png;
    }
    const bool video = !isAudioFormat(eo.format);
    if (video && !pr.subs.isEmpty()) {
        if (subMode == 1) {
            eo.subBurn = writeSubtitleBurn(pr.subs, pr.subStyle, QSize(eo.finalW(), eo.finalH()), pr.total(),
                                           job.tmp->filePath("subs"));
        } else if (subMode == 2 && eo.format != "gif") {
            QList<SubCue> cues = pr.subs;
            normalizeCues(cues, pr.total());
            QFile f(job.tmp->filePath("subs.srt"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(toSrt(cues).toUtf8());
                eo.subFile = f.fileName();
            }
        }
    }
    if (twoPass && eo.videoKbps > 0 && video && eo.format != "gif") {
        eo.passLog = job.tmp->filePath("pass");
        eo.pass = 1;
        job.passes << buildExport(ep, eo, out);
        eo.pass = 2;
        job.passes << buildExport(ep, eo, out);
    } else {
        eo.pass = 0;
        job.passes << buildExport(ep, eo, out);
    }
    return job;
}

bool runExportBlocking(const ExportJob& job, QString* err, int timeoutMs) {
    for (const QStringList& args : job.passes) {
        QProcess p;
        p.setWorkingDirectory(job.tmp->path());  // 2-Pass legt dort auch Hilfsdateien ab
        p.start(ffmpegPath(), args);
        if (!p.waitForFinished(timeoutMs) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            if (err) *err = QString::fromUtf8(p.readAllStandardError()).right(1500);
            p.kill();
            return false;
        }
    }
    return true;
}
