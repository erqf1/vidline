#include "model.h"

#include <cmath>

#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QMap>
#include <QRegularExpression>

const AspectFormat kAspects[kAspectCount] = {
    {0, 0, nullptr},
    {16, 9, "16:9 (YouTube)"},
    {9, 16, "9:16 (TikTok, Reels, Shorts)"},
    {1, 1, "1:1"},
    {4, 5, "4:5 (Instagram)"},
    {4, 3, "4:3"},
    {21, 9, "21:9"},
};

QSize canvasSize(int aspect, int natW, int natH) {
    if (natW <= 0 || natH <= 0) return {1920, 1080};
    const AspectFormat& f = kAspects[std::clamp(aspect, 0, kAspectCount - 1)];
    if (f.w == 0) return {natW, natH};
    const double base = std::min(natW, natH);
    auto even = [](double v) { return std::max(2, int(std::lround(v / 2)) * 2); };
    if (f.w >= f.h) return {even(base * f.w / f.h), even(base)};
    return {even(base), even(base * f.h / f.w)};
}

QString fmtTime(double t) {
    t = std::max(0.0, t);
    int m = int(t / 60);
    double s = t - m * 60;
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 4, 'f', 1, QChar('0'));
}

QString ffmpegPath() {
#ifdef Q_OS_WIN
    const QString name = "ffmpeg.exe";
#else
    const QString name = "ffmpeg";
#endif
    const QString dir = QCoreApplication::applicationDirPath();
    for (const QString& c : {dir + "/" + name, dir + "/../lib/vidline/" + name, dir + "/../Resources/" + name})
        if (QFileInfo::exists(c)) return QFileInfo(c).absoluteFilePath();
#ifdef Q_OS_MAC
    for (const char* c : {"/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg"})
        if (QFileInfo::exists(c)) return c;
#endif
    return "ffmpeg";
}

QString detectEncoder() {
    static QString cached;
    if (!cached.isEmpty()) return cached;
    for (const char* enc : {"h264_nvenc", "h264_qsv", "h264_amf", "h264_videotoolbox"}) {
        QProcess p;
        p.start(ffmpegPath(), {"-v", "error", "-f", "lavfi", "-i", "color=c=black:s=256x256:d=0.2",
                               "-c:v", enc, "-f", "null", "-"});
        if (p.waitForFinished(15000) && p.exitCode() == 0) return cached = enc;
    }
    return cached = "libx264";
}

MediaInfo probeMedia(const QString& path) {
    MediaInfo m;
    QProcess p;
    p.start(ffmpegPath(), {"-hide_banner", "-i", path});
    if (!p.waitForFinished(20000)) return m;
    const QString e = QString::fromUtf8(p.readAllStandardError());
    auto d = QRegularExpression(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))").match(e);
    if (d.hasMatch()) {
        m.duration = d.captured(1).toInt() * 3600 + d.captured(2).toInt() * 60 + d.captured(3).toDouble();
        m.ok = true;
    }
    m.hasAudio = e.contains(QRegularExpression(R"(Stream #\d+:\d+.*Audio:)"));
    // Cover-Bilder in Musikdateien ("attached pic") zählen nicht als Video
    static const QRegularExpression vre(R"(Stream #\d+:\d+.*Video:.*?(\d{2,5})x(\d{2,5}))");
    for (const QString& line : e.split('\n')) {
        if (line.contains("attached pic")) continue;
        auto v = vre.match(line);
        if (v.hasMatch()) { m.w = v.captured(1).toInt(); m.h = v.captured(2).toInt(); break; }
    }
    auto f = QRegularExpression(R"((\d+(?:\.\d+)?) fps)").match(e);
    if (f.hasMatch()) m.fps = f.captured(1).toDouble();
    return m;
}

static QString atempoChain(double sp) {
    QStringList parts;
    while (sp > 2.0) { parts << "atempo=2.0"; sp /= 2.0; }
    while (sp < 0.5) { parts << "atempo=0.5"; sp /= 0.5; }
    parts << QString("atempo=%1").arg(sp, 0, 'f', 5);
    return parts.join(",");
}

QStringList encArgs(const QString& enc, int q) {
    static const int qv[3] = {19, 24, 29};
    const QString v = QString::number(qv[std::clamp(q, 0, 2)]);
    if (enc == "h264_nvenc") return {"-c:v", enc, "-preset", "p5", "-rc", "vbr", "-cq", v, "-b:v", "0"};
    if (enc == "h264_videotoolbox") {
        static const char* qq[3] = {"75", "60", "45"};
        return {"-c:v", enc, "-q:v", qq[std::clamp(q, 0, 2)]};
    }
    if (enc == "h264_qsv") return {"-c:v", enc, "-preset", "medium", "-global_quality", v};
    if (enc == "h264_amf")
        return {"-c:v", enc, "-quality", "quality", "-rc", "cqp", "-qp_i", v, "-qp_p", QString::number(v.toInt() + 2),
                "-qp_b", QString::number(v.toInt() + 4)};
    static const int crf[3] = {18, 23, 28};
    return {"-c:v", "libx264", "-preset", "veryfast", "-crf", QString::number(crf[std::clamp(q, 0, 2)])};
}

bool isAudioFormat(const QString& f) { return f == "mp3" || f == "wav" || f == "m4a"; }

static int even(double v) { return std::max(2, int(std::lround(v / 2.0)) * 2); }

QString denoiseModelPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/lq.rnnn";
    QFile res(":/denoise/lq.rnnn");
    if (QFileInfo(path).size() != res.size()) {
        QFile::remove(path);
        res.copy(path);
    }
    return path;
}

namespace {
// Klicks (Tastatur/Maus) entfernen, dann RNNoise gegen Rauschen; Pfad für den Filtergraphen maskiert
QString denoiseFilter() {
    QString m = QDir::fromNativeSeparators(denoiseModelPath());
    m.replace("'", "\\'").replace(":", "\\:");
    return QString("adeclick,arnndn=m='%1',").arg(m);
}
}  // namespace

QStringList buildExport(const Project& pr, const ExportOptions& o, const QString& outPath) {
    auto n = [](double v, int d = 4) { return QString::number(v, 'f', d); };
    const double total = pr.total();
    const int W = o.width, H = o.height;
    const bool audioOnly = isAudioFormat(o.format), gif = o.format == "gif";
    const bool pass1 = o.pass == 1;  // erster Durchgang (2-Pass): nur das Bild analysieren, kein Ton, keine Datei
    const QString out = pass1 ? QString("-") : outPath;
    QStringList cmd = {"-hide_banner", "-y"};

    // Eingänge: Quellen, dann Bilder (auch vorab gerenderte Texte), dann Ton-Clips, dann Overlay-Videos
    for (const Source& s : pr.sources) cmd << "-i" << s.path;
    int nextIn = pr.sources.size();
    QMap<int, int> imgIn, vidIn;  // Index in pr.items -> Eingang
    if (!audioOnly)
        for (int i = 0; i < pr.items.size(); ++i) {
            const Item& it = pr.items[i];
            if (it.kind == Item::Blur || it.kind == Item::Video) continue;
            cmd << "-loop" << "1" << "-framerate" << n(o.fps, 3) << "-t" << n(total, 3) << "-i" << it.path;
            imgIn[i] = nextIn++;
        }
    QList<int> audIn;
    if (!gif && !pass1)
        for (const AudioClip& a : pr.audios) { cmd << "-i" << a.path; audIn << nextIn++; }
    const bool wantVideoAudio = !gif && !pass1;
    for (int i = 0; i < pr.items.size(); ++i) {
        const Item& it = pr.items[i];
        if (it.kind != Item::Video || it.t1 <= it.t0) continue;
        if (audioOnly && !(wantVideoAudio && it.hasAudio)) continue;
        cmd << "-ss" << n(it.srcStart, 3) << "-t" << n(it.t1 - it.t0, 3) << "-i" << it.path;
        vidIn[i] = nextIn++;
    }
    int subIn = -1, softSubIn = -1;
    if (!audioOnly && !o.subBurn.isEmpty()) {
        cmd << "-f" << "concat" << "-safe" << "0" << "-i" << o.subBurn;
        subIn = nextIn++;
    }
    if (!audioOnly && !gif && !pass1 && !o.subFile.isEmpty()) {
        cmd << "-i" << o.subFile;
        softSubIn = nextIn++;
    }

    bool videoAudio = false;
    for (const Piece& p : pr.pieces) videoAudio |= pr.sources.value(p.src).hasAudio;
    videoAudio = videoAudio && !gif && !pass1;
    bool overlayAudio = false;
    for (auto it = vidIn.cbegin(); it != vidIn.cend(); ++it) overlayAudio |= pr.items[it.key()].hasAudio;
    overlayAudio = overlayAudio && wantVideoAudio;
    const bool needAudio = !gif && !pass1 && (videoAudio || !pr.audios.isEmpty() || overlayAudio || audioOnly);
    const double mainGain = pr.trackGain(kMainTrack);

    QStringList f;
    const int cnt = pr.pieces.size();
    for (int i = 0; i < cnt; ++i) {
        const Piece& p = pr.pieces[i];
        if (!audioOnly && !pr.sources.value(p.src).video) {
            // Tondatei als Abschnitt: schwarzes Bild in der Länge des Abschnitts
            f << QString("color=c=black:s=%1x%2:r=%3:d=%4,setsar=1[v%5]").arg(W).arg(H).arg(n(o.fps, 3)).arg(n(p.outDur())).arg(i);
        } else if (!audioOnly) {
            QString v = QString("[%1:v]trim=start=%2:end=%3,setpts=(PTS-STARTPTS)/%4,")
                            .arg(p.src).arg(n(p.start)).arg(n(p.end)).arg(n(p.speed, 5));
            if (!p.transformed()) {
                v += QString("scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2:black,"
                             "setsar=1,fps=%3[v%4]").arg(W).arg(H).arg(n(o.fps, 3)).arg(i);
            } else {
                // Eingepasst * Größe, gedreht, auf schwarzer Fläche verschoben
                v += QString("scale=%1:%2:force_original_aspect_ratio=decrease,format=rgba")
                         .arg(even(W * p.scale)).arg(even(H * p.scale));
                if (std::abs(p.rot) > 1e-3)
                    v += QString(",rotate=%1:c=none:ow=rotw(%1):oh=roth(%1)").arg(n(p.rot * 3.14159265358979 / 180.0, 6));
                v += QString("[fg%1];color=c=black:s=%2x%3:r=%4[bg%1];[bg%1][fg%1]overlay=x=(W-w)/2+%5:y=(H-h)/2+%6:"
                             "shortest=1,setsar=1,fps=%4[v%1]")
                         .arg(i).arg(W).arg(H).arg(n(o.fps, 3)).arg(n(p.px * W, 1)).arg(n(p.py * H, 1));
            }
            f << v;
        }
        if (videoAudio) {
            if (pr.sources.value(p.src).hasAudio)
                // %8: Rauschunterdrückung (Klicks raus, dann RNNoise) - nur wenn angehakt
                // apad: ist der Ton kürzer als das Bild, mit Stille auffüllen (sonst verrutscht alles danach)
                f << QString("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS,%4,volume=%6,aresample=48000,%8"
                             "aformat=sample_fmts=fltp:channel_layouts=stereo,apad=whole_dur=%7,atrim=end=%7[a%5]")
                         .arg(p.src).arg(n(p.start)).arg(n(p.end)).arg(atempoChain(p.speed)).arg(i)
                         .arg(n(p.volume * mainGain, 3)).arg(n(p.outDur())).arg(p.denoise ? denoiseFilter() : QString());
            else
                f << QString("anullsrc=r=48000:cl=stereo:d=%1,aformat=sample_fmts=fltp[a%2]").arg(n(p.outDur())).arg(i);
        }
    }
    QString ins;
    for (int i = 0; i < cnt; ++i)
        ins += (audioOnly ? QString() : QString("[v%1]").arg(i)) + (videoAudio ? QString("[a%1]").arg(i) : QString());
    if (!audioOnly)
        f << QString("%1concat=n=%2:v=1:a=%3[vc]%4").arg(ins).arg(cnt).arg(videoAudio ? 1 : 0).arg(videoAudio ? "[abase]" : "");
    else if (videoAudio)
        f << QString("%1concat=n=%2:v=0:a=1[abase]").arg(ins).arg(cnt);

    // Darüber: erst die Video-Spuren V2, V3 … (Bild-im-Bild, Overlays), dann Bilder, Texte, Unschärfe in ihrer Reihenfolge
    if (!audioOnly) {
        QList<int> order;
        for (int tid : pr.trackIds(false)) {
            if (tid == kMainTrack) continue;
            for (const Item* c : pr.trackClips(tid)) order << int(c - pr.items.constData());
        }
        for (int i = 0; i < pr.items.size(); ++i)
            if (!pr.track(pr.items[i].track) || pr.items[i].track == kMainTrack) order << i;
        QString cur = "vc";
        int k = 0;
        for (int idx : order) {
            const Item& it = pr.items[idx];
            if (it.t1 <= it.t0) continue;
            if (it.kind == Item::Video && !vidIn.contains(idx)) continue;
            int x = int(it.x * W), y = int(it.y * H);
            int w = std::max(2, int(it.w * W)), h = std::max(2, int(it.h * H));
            QString en = QString("enable='between(t,%1,%2)'").arg(n(it.t0, 3)).arg(n(it.t1, 3));
            const QString alpha = it.media() && it.opacity < 0.999
                                      ? QString(",colorchannelmixer=aa=%1").arg(n(std::clamp(it.opacity, 0.0, 1.0), 3))
                                      : QString();
            ++k;
            if (it.kind == Item::Blur) {
                x = std::clamp(x, 0, W - 2);
                y = std::clamp(y, 0, H - 2);
                w = std::min(w, W - x);
                h = std::min(h, H - y);
                const double sigma = it.strength * H / 1080.0;
                f << QString("[%1]split[s%2a][s%2b];[s%2b]crop=%3:%4:%5:%6,gblur=sigma=%7:steps=2[bl%2];"
                             "[s%2a][bl%2]overlay=%5:%6:%8[o%2]")
                         .arg(cur).arg(k).arg(w).arg(h).arg(x).arg(y).arg(n(std::max(1.0, sigma), 1)).arg(en);
            } else if (it.kind == Item::Video) {
                // Video zeitlich an seine Stelle schieben; vorher und nachher bleibt das Bild darunter
                f << QString("[%1:v]setpts=PTS-STARTPTS+%2/TB,scale=%3:%4,format=rgba%5[pv%6];"
                             "[%7][pv%6]overlay=%8:%9:eof_action=pass:%10[o%6]")
                         .arg(vidIn[idx]).arg(n(it.t0, 3)).arg(even(w)).arg(even(h)).arg(alpha).arg(k).arg(cur)
                         .arg(x).arg(y).arg(en);
            } else {
                f << QString("[%1:v]format=rgba,scale=%2:%3%9[im%4];[%5][im%4]overlay=%6:%7:%8[o%4]")
                         .arg(imgIn.value(idx)).arg(w).arg(h).arg(k).arg(cur).arg(x).arg(y).arg(en).arg(alpha);
            }
            cur = QString("o%1").arg(k);
        }
        // Zielgröße einer Vorlage (z. B. 9:16): Rand, Zuschnitt oder unscharfer Rand
        const int TW = o.finalW(), TH = o.finalH();
        if (TW != W || TH != H) {
            if (o.fit == 1) {
                f << QString("[%1]scale=%2:%3:force_original_aspect_ratio=increase,crop=%2:%3,setsar=1[fit]").arg(cur).arg(TW).arg(TH);
            } else if (o.fit == 2) {
                const int bw = even(TW / 4.0), bh = even(TH / 4.0);
                f << QString("[%1]split[fa][fb];[fa]scale=%2:%3:force_original_aspect_ratio=increase,crop=%2:%3,"
                             "gblur=sigma=10,eq=brightness=-0.06,scale=%4:%5[fbg];"
                             "[fb]scale=%4:%5:force_original_aspect_ratio=decrease[ffg];"
                             "[fbg][ffg]overlay=(W-w)/2:(H-h)/2,setsar=1[fit]")
                         .arg(cur).arg(bw).arg(bh).arg(TW).arg(TH);
            } else {
                f << QString("[%1]scale=%2:%3:force_original_aspect_ratio=decrease,pad=%2:%3:(ow-iw)/2:(oh-ih)/2:black,"
                             "setsar=1[fit]").arg(cur).arg(TW).arg(TH);
            }
            cur = "fit";
        }
        // Untertitel einbrennen: Bilder in Zielgröße, jedes so lange sichtbar wie sein Untertitel
        if (subIn >= 0) {
            f << QString("[%1:v]format=rgba[subs];[%2][subs]overlay=0:0:eof_action=repeat[sb]").arg(subIn).arg(cur);
            cur = "sb";
        }
        if (gif) {
            const int gw = even(std::min(TW, std::max(16, o.gifWidth))), gh = even(double(TH) * gw / TW);
            f << QString("[%1]fps=%2,scale=%3:%4:flags=lanczos,split[g1][g2];[g1]palettegen=stats_mode=diff[pal];"
                         "[g2][pal]paletteuse=dither=bayer:bayer_scale=4[vout]")
                     .arg(cur).arg(n(std::min(o.fps, o.gifFps), 3)).arg(gw).arg(gh);
        } else {
            f << QString("[%1]format=yuv420p[vout]").arg(cur);
        }
    }

    // Ton: Basis (Video-Ton oder Stille) + Ton-Clips + Ton der Overlay-Videos mischen
    if (needAudio) {
        if (!videoAudio)
            f << QString("anullsrc=r=48000:cl=stereo:d=%1,aformat=sample_fmts=fltp[abase]").arg(n(total, 3));
        QString mix = "[abase]";
        int mixN = 1;
        for (int i = 0; i < pr.audios.size(); ++i) {
            const AudioClip& a = pr.audios[i];
            const double vol = a.volume * pr.trackGain(pr.audioTrackOf(a));
            f << QString("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS,volume=%4,aresample=48000,%7"
                         "aformat=sample_fmts=fltp:channel_layouts=stereo,adelay=%5:all=1[c%6]")
                     .arg(audIn[i]).arg(n(a.srcStart)).arg(n(a.srcStart + a.dur)).arg(n(vol, 3))
                     .arg(int(a.t0 * 1000)).arg(i).arg(a.denoise ? denoiseFilter() : QString());
            mix += QString("[c%1]").arg(i);
            ++mixN;
        }
        for (auto it = vidIn.cbegin(); it != vidIn.cend(); ++it) {
            const Item& c = pr.items[it.key()];
            if (!c.hasAudio) continue;
            f << QString("[%1:a]asetpts=PTS-STARTPTS,volume=%2,aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo,"
                         "apad=whole_dur=%3,atrim=end=%3,adelay=%4:all=1[cv%5]")
                     .arg(it.value()).arg(n(c.volume * pr.trackGain(c.track), 3)).arg(n(c.t1 - c.t0)).arg(int(c.t0 * 1000))
                     .arg(it.key());
            mix += QString("[cv%1]").arg(it.key());
            ++mixN;
        }
        if (mixN == 1) f.replaceInStrings("[abase]", "[aout]");
        else f << QString("%1amix=inputs=%2:duration=first:normalize=0[aout]").arg(mix).arg(mixN);
    }

    cmd << "-filter_complex" << f.join(";");
    if (!audioOnly) cmd << "-map" << "[vout]";
    if (needAudio) cmd << "-map" << "[aout]";
    if (softSubIn >= 0) cmd << "-map" << QString("%1:0").arg(softSubIn);
    static const char* abr[3] = {"320k", "192k", "128k"};
    const QString ab = o.audioKbps > 0 ? QString("%1k").arg(o.audioKbps) : QString(abr[std::clamp(o.quality, 0, 2)]);
    if (audioOnly) {
        if (o.format == "mp3") cmd << "-c:a" << "libmp3lame" << "-b:a" << ab;
        else if (o.format == "wav") cmd << "-c:a" << "pcm_s16le";
        else cmd << "-c:a" << "aac" << "-b:a" << ab << "-movflags" << "+faststart";
        if (o.sampleRate > 0) cmd << "-ar" << QString::number(o.sampleRate);
        if (o.mono) cmd << "-ac" << "1";
    } else if (gif) {
        cmd << "-loop" << "0";
    } else {
        cmd << "-r" << n(o.fps, 3);
        if (o.videoKbps > 0) {
            // Ziel-Dateigröße: feste Bitrate mit x264, auf Wunsch in zwei Durchgängen (trifft die Größe genau)
            cmd << "-c:v" << "libx264" << "-preset" << "medium" << "-b:v" << QString("%1k").arg(o.videoKbps)
                << "-maxrate" << QString("%1k").arg(int(o.videoKbps * 1.5)) << "-bufsize" << QString("%1k").arg(o.videoKbps * 2);
            if (o.pass > 0) cmd << "-pass" << QString::number(o.pass) << "-passlogfile" << o.passLog;
        } else {
            cmd << encArgs(o.encoder, o.quality);
        }
        if (needAudio) cmd << "-c:a" << "aac" << "-b:a" << (o.audioKbps > 0 ? ab : QString("192k"));
        if (softSubIn >= 0) cmd << "-c:s" << (o.format == "mkv" ? "srt" : "mov_text");
        if (!pass1 && (o.format == "mp4" || o.format == "mov")) cmd << "-movflags" << "+faststart";
    }
    if (pass1) cmd << "-f" << "null";
    cmd << "-t" << n(total, 3) << "-progress" << "pipe:1" << "-nostats" << out;
    return cmd;
}
