// Entwickler-Tests für Vidline 1.17: Spuren (--trackstest), Export-Vorlagen (--presettest), Untertitel (--subtest).
// Laufen unsichtbar (QT_QPA_PLATFORM=offscreen), erzeugen ihre Testvideos selbst mit ffmpeg (testsrc + Sinuston),
// schreiben <ordner>/<name>-ergebnis.txt und Bilder und beenden das Programm mit Exit-Code 1, wenn etwas fehlschlägt.
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QLineEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include "dialogs.h"
#include "i18n.h"
#include "mainwindow.h"
#include "plugins.h"
#include "presets.h"
#include "subtitles.h"
#include "testdata/legacy_cases.h"
#include "timeline.h"

namespace {
struct Checker {
    QString dir, name;
    QStringList lines;
    int fails = 0;
    void check(const QString& what, bool ok, const QString& info = QString()) {
        lines << QString("%1 %2%3").arg(ok ? "ok  " : "FEHL", what, info.isEmpty() ? QString() : " (" + info + ")");
        fails += !ok;
        fprintf(stderr, "%s %s %s\n", ok ? "ok  " : "FEHL", qPrintable(what), qPrintable(info));
    }
    void note(const QString& s) { lines << "    " + s; }
    void finish() {
        QFile f(QDir(dir).filePath(name + "-ergebnis.txt"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write((lines.join('\n') + QString("\n%1 Prüfungen, %2 fehlgeschlagen\n").arg(lines.filter(QRegularExpression("^(ok|FEHL)")).size()).arg(fails)).toUtf8());
        const int code = fails ? 1 : 0;
        QTimer::singleShot(100, qApp, [code] { QCoreApplication::exit(code); });
    }
};

bool waitFor(const std::function<bool()>& cond, int ms) {
    QElapsedTimer t;
    t.start();
    while (!cond()) {
        if (t.elapsed() > ms) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return true;
}

void pause(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

// ffmpeg im Vordergrund; true bei Erfolg
bool ff(const QStringList& args, QString* err = nullptr, int ms = 300000) {
    QProcess p;
    p.start(ffmpegPath(), QStringList{"-hide_banner", "-v", "error", "-y"} + args);
    const bool ok = p.waitForFinished(ms) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    if (err) *err = QString::fromUtf8(p.readAllStandardError()).right(600);
    return ok;
}

bool makeVideo(const QString& out, const QString& video, int w, int h, double dur, double freq) {
    QStringList a = {"-f", "lavfi", "-i", QString("%1%2s=%3x%4:r=30:d=%5").arg(video, video.contains('=') ? ":" : "=").arg(w).arg(h).arg(dur)};
    if (freq > 0) a << "-f" << "lavfi" << "-i" << QString("sine=f=%1:d=%2:sample_rate=48000").arg(freq).arg(dur);
    a << "-c:v" << "libx264" << "-preset" << "ultrafast" << "-pix_fmt" << "yuv420p";
    if (freq > 0) a << "-c:a" << "aac" << "-shortest";
    a << out;
    return ff(a);
}

QImage frameAt(const QString& file, double t, const QString& dir) {
    const QString png = QDir(dir).filePath(QString("frame-%1.png").arg(qHash(file + QString::number(t))));
    if (!ff({"-ss", QString::number(t, 'f', 3), "-i", file, "-frames:v", "1", png})) return {};
    return QImage(png);
}

// Pegel (mean_volume dB) eines Frequenzbands in einer Tondatei
double bandLevel(const QString& file, double freq) {
    QProcess p;
    p.start(ffmpegPath(), {"-hide_banner", "-i", file, "-af",
                           QString("bandpass=f=%1:width_type=q:w=8,volumedetect").arg(freq), "-f", "null", "-"});
    p.waitForFinished(60000);
    const auto m = QRegularExpression(R"(mean_volume:\s*(-?[\d.]+|-inf))").match(QString::fromUtf8(p.readAllStandardError()));
    if (!m.hasMatch() || m.captured(1) == "-inf") return -120;
    return m.captured(1).toDouble();
}

QString streams(const QString& file) {
    QProcess p;
    p.start(ffmpegPath(), {"-hide_banner", "-i", file});
    p.waitForFinished(20000);
    return QString::fromUtf8(p.readAllStandardError());
}

bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }

bool colorNear(QRgb c, int r, int g, int b, int tol) {
    return std::abs(qRed(c) - r) <= tol && std::abs(qGreen(c) - g) <= tol && std::abs(qBlue(c) - b) <= tol;
}

// hellster Punkt eines Bereichs (0..255)
int maxLuma(const QImage& img, QRect r) {
    r &= img.rect();
    int m = 0;
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x) m = std::max(m, qGray(img.pixel(x, y)));
    return m;
}

// Helligkeit eines Bereichs (0..255)
double meanLuma(const QImage& img, QRect r) {
    r &= img.rect();
    double s = 0;
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); y += 2)
        for (int x = r.left(); x <= r.right(); x += 2) {
            s += qGray(img.pixel(x, y));
            ++n;
        }
    return n ? s / n : 0;
}
}  // namespace

// ---------------------------------------------------------------- Spuren
void MainWindow::tracksTest(const QString& dir) {
    silent_ = true;
    audio_->setMuted(true);
    QDir().mkpath(dir);
    auto* c = new Checker{dir, "spuren"};
    QTemporaryDir tmp;
    const QString main = tmp.filePath("Beispiel.mp4"), pip = tmp.filePath("Kamera.mp4");
    const QString music = tmp.filePath("Musik.wav"), voice = tmp.filePath("Sprache.wav"), logo = tmp.filePath("Logo.png");
    c->check("Testvideos erzeugt",
             makeVideo(main, "color=c=0x404040", 640, 360, 4, 440) && makeVideo(pip, "color=c=red", 320, 180, 4, 880) &&
                 ff({"-f", "lavfi", "-i", "sine=f=220:d=4", music}) && ff({"-f", "lavfi", "-i", "sine=f=1500:d=4", voice}));
    {
        QImage im(100, 100, QImage::Format_RGB32);
        im.fill(QColor(0, 0, 255));
        im.save(logo);
    }

    // Alt-Projekte: genau dieselben ffmpeg-Befehle wie Vidline 1.16
    {
        QFile g(":/tests/golden-export.txt");
        g.open(QIODevice::ReadOnly);
        const QStringList golden = QString::fromUtf8(g.readAll()).split('\n', Qt::SkipEmptyParts);
        const auto cases = legacyCases();
        int same = 0;
        for (int i = 0; i < cases.size(); ++i)
            same += golden.value(i) == cases[i].first + "\t" + cases[i].second.join(QChar(0x1f));
        c->check("Alte Projekte: Export-Befehle unverändert wie in 1.16", same == cases.size() && golden.size() == cases.size(),
                 QString("%1/%2").arg(same).arg(cases.size()));
    }

    openFile(main);
    c->check("Video geladen", waitFor([this] { return !pr_.pieces.isEmpty() && canvasSet_; }, 20000));
    btnEdit_->setChecked(true);
    resize(1400, 900);
    pause(300);
    c->check("Neue Projekte: Spuren V1 und A1", pr_.tracks.size() == 2 && pr_.trackName(kMainTrack) == "V1" &&
                                                     pr_.trackName(pr_.trackIds(true).value(0)) == "A1");
    c->check("Spurköpfe sichtbar", trackHeader_ && trackHeader_->isVisibleTo(this));

    // Bild-im-Bild auf V2, Bild auf V3 (halb durchsichtig), Musik auf A1, Sprache auf neuer A2
    const int v2 = pr_.addTrack(false);
    insertMediaOnTrack(pip, 1.0, v2);
    const Item* pv = nullptr;
    for (const Item& it : pr_.items) if (it.kind == Item::Video) pv = &it;
    c->check("Overlay-Video auf V2", pv && pv->track == v2 && pr_.trackName(v2) == "V2" && pv->hasAudio && near(pv->t0, 1.0, 0.01),
             pv ? QString("Spur %1, t0 %2").arg(pr_.trackName(pv->track)).arg(pv->t0) : QString("fehlt"));
    const int pvId = pv ? pv->id : 0;
    c->check("Overlay-Video bis Videoende gekürzt", pv && near(pv->t1, 4.0, 0.05), pv ? QString::number(pv->t1) : QString());
    c->check("Player für das Overlay-Video", ovPlayers_.size() == 1 && ovPlayers_.count(pvId));
    if (Item* it = pr_.item(pvId)) { it->x = 0.5; it->y = 0.5; it->w = 0.4; it->h = 0.4; it->volume = 0.0; }
    const int v3 = pr_.addTrack(false);
    seek(0.5);
    insertMediaOnTrack(logo, 0.5, v3);
    Item* lg = pr_.item(selItem_);
    c->check("Bild auf V3", lg && lg->kind == Item::Image && lg->track == v3);
    if (lg) { lg->x = 0.05; lg->y = 0.1; lg->w = 0.2; lg->h = 0.2 * 640 / 360.0; lg->opacity = 0.5; lg->t0 = 0; lg->t1 = 4; }
    const int a1 = pr_.trackIds(true).value(0);
    insertMediaOnTrack(music, 0.0, a1);
    const int musicId = selAudio_;
    c->check("Musik auf A1", pr_.audio(musicId) && pr_.audioTrackOf(*pr_.audio(musicId)) == a1);
    seek(0.0);
    const MediaInfo vi = probeMedia(voice);
    addAudioFile(voice, vi);
    const int voiceId = selAudio_;
    const int a2 = pr_.audio(voiceId) ? pr_.audioTrackOf(*pr_.audio(voiceId)) : 0;
    c->check("Überlappender Ton bekommt eine eigene Spur A2", a2 && a2 != a1 && pr_.trackName(a2) == "A2");
    modelEdited();

    // Zeitleiste: Zeilen in der richtigen Reihenfolge
    {
        const auto rs = timeline_->rows();
        QStringList kinds;
        for (const auto& r : rs) kinds << QString("MVLSA").mid(int(r.kind), 1);
        c->check("Zeilen: V1, V2, V3, A1, A2", kinds.join("") == "MVVAA", kinds.join(""));
    }
    // Verschieben zwischen Spuren + Rückgängig
    c->check("Ton-Clip auf A1 verschieben", moveToTrack('a', voiceId, a1) && pr_.audioTrackOf(*pr_.audio(voiceId)) == a1);
    doUndo();
    c->check("Rückgängig: wieder auf A2", pr_.audio(voiceId) && pr_.audioTrackOf(*pr_.audio(voiceId)) == a2);
    doRedo();
    c->check("Wiederholen: wieder auf A1", pr_.audio(voiceId) && pr_.audioTrackOf(*pr_.audio(voiceId)) == a1);
    doUndo();
    c->check("Bild auf eine neue Video-Spur", moveToTrack('i', lg ? lg->id : 0, -1) && pr_.trackIds(false).size() == 4);
    doUndo();
    lg = pr_.item(lg ? lg->id : 0);
    c->check("Rückgängig: Bild wieder auf V3, Spur weg", lg && lg->track == v3 && pr_.trackIds(false).size() == 3);
    // Abschnitt aufs Overlay und zurück ins Hauptvideo
    seek(2.0);
    selectPiece(0);
    doSplit();
    c->check("Teilen: zwei Abschnitte", pr_.pieces.size() == 2);
    const int itemsBefore = pr_.items.size();
    c->check("Abschnitt 2 auf V2 legen", moveToTrack('p', 1, v2) && pr_.pieces.size() == 1 && pr_.items.size() == itemsBefore + 1);
    const Item* moved = pr_.items.isEmpty() ? nullptr : &pr_.items.last();
    c->check("…als Video mit gleicher Quelle und Startpunkt", moved && moved->kind == Item::Video && near(moved->srcStart, 2.0, 0.05) &&
                                                               moved->track == v2,
             moved ? QString::number(moved->srcStart) : QString());
    if (moved) moveToTrack('i', moved->id, kMainTrack);
    c->check("Zurück ins Hauptvideo", pr_.pieces.size() == 2 && near(pr_.total(), 4.0, 0.05), QString::number(pr_.total()));
    doUndo();
    doUndo();
    doUndo();
    c->check("Rückgängig bis vor dem Teilen", pr_.pieces.size() == 1 && pr_.items.size() == itemsBefore);

    // Stumm / Solo / Lautstärke
    Track* ta1 = pr_.track(a1);
    Track* ta2 = pr_.track(a2);
    ta2->solo = true;
    c->check("Solo A2: nur A2 hörbar", pr_.trackGain(a2) == 1.0 && pr_.trackGain(a1) == 0.0 && pr_.trackGain(kMainTrack) == 0.0);
    ta2->solo = false;
    ta1->mute = true;
    ta2->volume = 0.5;
    c->check("Stumm A1, A2 auf 50 %", pr_.trackGain(a1) == 0.0 && near(pr_.trackGain(a2), 0.5, 1e-9) && pr_.trackGain(kMainTrack) == 1.0);
    tracksChanged();
    ta1->mute = false;
    ta2->volume = 1.0;
    tracksChanged();

    // Bilder für Webseite/Prüfung
    selectItem(pvId);
    seek(2.0);
    pause(1200);
    grab().save(QDir(dir).filePath("spuren-editor.png"));
    editBottom_->grab().save(QDir(dir).filePath("spuren-zeitleiste.png"));
    selectTrack(a2);
    inspCard_->grab().save(QDir(dir).filePath("spuren-eigenschaften.png"));

    // Echter Export: alles gemischt
    ensureProbed();
    auto exportTo = [&](const QString& file, const QString& fmt) {
        ExportOptions o;
        o.width = 640;
        o.height = 360;
        o.fps = 30;
        o.format = fmt;
        ExportJob job = prepareExport(pr_, o, file, false, 0);
        QString err;
        const bool ok = runExportBlocking(job, &err);
        if (!ok) c->note(err.left(400));
        return ok;
    };
    const QString out = QDir(tmp.path()).filePath("gemischt.mp4");
    c->check("Export mit 3 Video- und 2 Ton-Spuren", exportTo(out, "mp4"));
    const MediaInfo mo = probeMedia(out);
    c->check("Export: 640×360, 4 s, mit Ton", mo.w == 640 && mo.h == 360 && near(mo.duration, 4.0, 0.15) && mo.hasAudio,
             QString("%1×%2 %3 s").arg(mo.w).arg(mo.h).arg(mo.duration));
    const QImage f05 = frameAt(out, 0.5, tmp.path()), f20 = frameAt(out, 2.0, tmp.path());
    c->check("Bild-im-Bild erst ab 1 s", !f05.isNull() && colorNear(f05.pixel(450, 250), 64, 64, 64, 14),
             f05.isNull() ? QString() : QColor(f05.pixel(450, 250)).name());
    c->check("Bild-im-Bild rot unten rechts bei 2 s", !f20.isNull() && colorNear(f20.pixel(450, 250), 230, 30, 30, 50),
             f20.isNull() ? QString() : QColor(f20.pixel(450, 250)).name());
    c->check("Logo mit 50 % Deckkraft über Grau", !f20.isNull() && colorNear(f20.pixel(60, 70), 32, 32, 160, 30),
             f20.isNull() ? QString() : QColor(f20.pixel(60, 70)).name());
    c->check("Hauptvideo frei an anderer Stelle", !f20.isNull() && colorNear(f20.pixel(300, 300), 64, 64, 64, 14),
             f20.isNull() ? QString() : QColor(f20.pixel(300, 300)).name());

    // Ton: Mischung, Stumm und Solo wirken im Export
    const QString wAll = tmp.filePath("alle.wav"), wSolo = tmp.filePath("solo.wav"), wMute = tmp.filePath("stumm.wav");
    exportTo(wAll, "wav");
    pr_.track(a2)->solo = true;
    exportTo(wSolo, "wav");
    pr_.track(a2)->solo = false;
    for (Track& t : pr_.tracks) t.mute = true;
    exportTo(wMute, "wav");
    for (Track& t : pr_.tracks) t.mute = false;
    const double all220 = bandLevel(wAll, 220), all440 = bandLevel(wAll, 440), all1500 = bandLevel(wAll, 1500);
    const double solo220 = bandLevel(wSolo, 220), solo1500 = bandLevel(wSolo, 1500), solo440 = bandLevel(wSolo, 440);
    c->check("Mischung enthält Hauptvideo, Musik und Sprache", all220 > -40 && all440 > -40 && all1500 > -40,
             QString("220 Hz %1 dB, 440 Hz %2 dB, 1500 Hz %3 dB").arg(all220).arg(all440).arg(all1500));
    c->check("Solo A2: nur die Sprache", solo1500 > -40 && solo220 < solo1500 - 25 && solo440 < solo1500 - 25,
             QString("1500 Hz %1 dB, 220 Hz %2 dB, 440 Hz %3 dB").arg(solo1500).arg(solo220).arg(solo440));
    c->check("Alle Spuren stumm: Stille", bandLevel(wMute, 440) < -80 && bandLevel(wMute, 1500) < -80);
    c->finish();
}

// ---------------------------------------------------------------- Export-Vorlagen
void MainWindow::presetTest(const QString& dir) {
    silent_ = true;
    audio_->setMuted(true);
    QDir().mkpath(dir);
    auto* c = new Checker{dir, "vorlagen"};
    QTemporaryDir tmp;
    const QString src = tmp.filePath("Beispiel.mp4");
    c->check("Testvideo 1280×720, 3 s", makeVideo(src, "testsrc2", 1280, 720, 3, 440));

    // Rechnen ohne Export
    c->check("Bitrate aus Länge: 10 MB / 60 s, 96 kbit/s Ton", sizeTargetVideoKbps(10e6, 60, 96) == 1170,
             QString::number(sizeTargetVideoKbps(10e6, 60, 96)));
    c->check("Bildfläche bei Rand 16:9 -> 9:16", compositionSize(1280, 720, 1080, 1920, 0) == QSize(1080, 608));
    c->check("Bildfläche bei Zuschnitt 16:9 -> 9:16", compositionSize(1280, 720, 1080, 1920, 1) == QSize(3414, 1920));
    c->check("Gleiches Seitenverhältnis: keine Umrechnung", compositionSize(1280, 720, 1920, 1080, 0) == QSize(1920, 1080));
    {
        const ExportPreset* d = findPreset(builtinPresets(), "discord10");
        const ExportPlan lng = planPreset(*d, 1920, 1080, 60, 600, true);
        c->check("Discord 10 MB, 10 min: kleinere Auflösung, 30 fps", lng.o.finalH() <= 360 && lng.o.fps == 30 && lng.twoPass,
                 QString("%1×%2 %3 kbit/s").arg(lng.o.finalW()).arg(lng.o.finalH()).arg(lng.o.videoKbps));
        const ExportPlan vlong = planPreset(*d, 1920, 1080, 30, 3600, true);
        c->check("Discord 10 MB, 1 h: als zu lang erkannt", vlong.tooLong);
        const ExportPlan sh = planPreset(*d, 1920, 1080, 30, 20, true);
        c->check("Discord 10 MB, 20 s: 720p", sh.o.finalH() == 720 && sh.o.videoKbps > 3000, QString::number(sh.o.videoKbps));
    }
    // Eigene Vorlagen
    {
        ExportPreset p;
        p.name = "Mein Kanal";
        p.w = 1280;
        p.h = 720;
        p.fit = 2;
        p.maxMB = 25;
        p.format = "mkv";
        p.quality = 0;
        saveCustomPreset(p);
        const ExportPreset* got = findPreset(customPresets(), "custom:Mein Kanal");
        c->check("Eigene Vorlage gespeichert und gelesen", got && got->w == 1280 && got->fit == 2 && got->maxMB == 25 &&
                                                             got->format == "mkv" && !got->builtin);
        saveCustomPreset(p);
        c->check("Gleicher Name überschreibt", customPresets().size() == 1);
        c->check("JSON hin und zurück", presetFromJson(presetToJson(p)).name == "Mein Kanal");
        c->check("Unsinn im JSON wird begrenzt", presetFromJson(R"({"name":"x","w":99999,"fit":7,"format":"exe"})").w == 7680 &&
                                                     presetFromJson(R"({"name":"x","fit":7,"format":"exe"})").format == "mp4");
        removeCustomPreset("Mein Kanal");
        c->check("Eigene Vorlage gelöscht", customPresets().isEmpty());
    }

    openFile(src);
    c->check("Video geladen", waitFor([this] { return !pr_.pieces.isEmpty() && canvasSet_; }, 20000));
    ensureProbed();
    const double dur = pr_.total();

    // Echter Export je Vorlage
    for (const ExportPreset& p : builtinPresets()) {
        const ExportPlan plan = planPreset(p, pr_.vw, pr_.vh, 30, dur, true);
        ExportOptions o = plan.o;
        o.encoder = "libx264";
        const QString out = tmp.filePath(p.id + "." + o.format);
        ExportJob job = prepareExport(pr_, o, out, plan.twoPass, 0);
        QString err;
        QElapsedTimer t;
        t.start();
        const bool ok = runExportBlocking(job, &err);
        const MediaInfo mi = probeMedia(out);
        const qint64 bytes = QFileInfo(out).size();
        int ew = o.finalW(), eh = o.finalH();
        if (o.format == "gif") {
            ew = std::max(2, int(std::lround(std::min(o.finalW(), o.gifWidth) / 2.0)) * 2);
            eh = std::max(2, int(std::lround(double(o.finalH()) * ew / o.finalW() / 2.0)) * 2);
        }
        c->check(QString("%1: Export").arg(p.title()), ok && bytes > 0, ok ? QString("%1 s").arg(t.elapsed() / 1000.0, 0, 'f', 1) : err.left(300));
        c->check(QString("%1: Auflösung %2×%3").arg(p.title()).arg(ew).arg(eh), mi.w == ew && mi.h == eh,
                 QString("%1×%2").arg(mi.w).arg(mi.h));
        c->check(QString("%1: Länge").arg(p.title()), near(mi.duration, dur, 0.2), QString::number(mi.duration));
        if (plan.maxBytes > 0)
            c->check(QString("%1: Datei unter %2 MB").arg(p.title()).arg(p.maxMB), bytes <= plan.maxBytes && plan.twoPass,
                     QString("%1 MB").arg(bytes / 1e6, 0, 'f', 2));
        else
            c->note(QString("%1: %2 MB").arg(p.title()).arg(bytes / 1e6, 0, 'f', 2));
        if (p.id == "tiktok") {
            const QImage f = frameAt(out, 1.0, tmp.path());
            c->check("TikTok mit schwarzem Rand: oben schwarz, Mitte Bild",
                     !f.isNull() && meanLuma(f, QRect(0, 0, 1080, 300)) < 8 && meanLuma(f, QRect(0, 800, 1080, 300)) > 40);
        }
        if (p.id == "gif") c->check("GIF-Datei", streams(out).contains("gif"));
    }
    // Zuschnitt und unscharfer Rand
    for (int fit : {1, 2}) {
        ExportPreset p = *findPreset(builtinPresets(), "tiktok");
        p.fit = fit;
        ExportOptions o = planPreset(p, pr_.vw, pr_.vh, 30, dur, true).o;
        const QString out = tmp.filePath(QString("tiktok-fit%1.mp4").arg(fit));
        QString err;
        const bool ok = runExportBlocking(prepareExport(pr_, o, out, false, 0), &err);
        const QImage f = frameAt(out, 1.0, tmp.path());
        const MediaInfo mi = probeMedia(out);
        c->check(fit == 1 ? "TikTok zugeschnitten: 1080×1920, oben Bild" : "TikTok unscharfer Rand: 1080×1920, oben nicht schwarz",
                 ok && mi.w == 1080 && mi.h == 1920 && !f.isNull() && meanLuma(f, QRect(0, 0, 1080, 300)) > 20,
                 f.isNull() ? err.left(200) : QString("Helligkeit oben %1").arg(meanLuma(f, QRect(0, 0, 1080, 300)), 0, 'f', 0));
    }
    // Zielgröße genau getroffen: schweres Rauschen, 8 s, höchstens 1 MB
    {
        const QString noise = tmp.filePath("rauschen.mp4");
        ff({"-f", "lavfi", "-i", "nullsrc=s=1280x720:r=30:d=8,geq=random(1)*255:128:128", "-f", "lavfi", "-i", "sine=f=500:d=8",
            "-c:v", "libx264", "-preset", "ultrafast", "-crf", "10", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", noise});
        openFile(noise);
        waitFor([this] { return !pr_.pieces.isEmpty() && canvasSet_; }, 20000);
        ensureProbed();
        ExportPreset p;
        p.name = "1 MB";
        p.maxMB = 1;
        p.audioKbps = 64;
        p.builtin = false;
        const ExportPlan plan = planPreset(p, pr_.vw, pr_.vh, 30, pr_.total(), true);
        const QString out = tmp.filePath("ziel1mb.mp4");
        QString err;
        const bool ok = runExportBlocking(prepareExport(pr_, plan.o, out, true, 0), &err);
        const qint64 b = QFileInfo(out).size();
        c->check("2-Pass: höchstens 1 MB bei schwerem Rauschen", ok && b <= 1000000, QString("%1 Bytes").arg(b));
        c->check("2-Pass: Größe gut ausgenutzt (≥ 75 %)", b >= 750000, QString("%1 %").arg(b / 10000.0, 0, 'f', 0));
        openFile(src);
        waitFor([this] { return !pr_.pieces.isEmpty() && canvasSet_; }, 20000);
    }
    // Dialog: Vorschau der Zielgröße und Dateigröße, Bilder
    {
        ExportDialog dlg(this, pr_.vw, pr_.vh, 30, pr_.sources[0].path);
        dlg.setEstimateInputs(pr_.total(), pr_.sources[0].duration, pr_.natW, pr_.natH, true);
        dlg.selectPreset("discord10");
        // keine echten Pfade in den Bildern
        const auto edits = dlg.findChildren<QLineEdit*>();
        if (edits.size() >= 2) {
            edits[0]->setText("Urlaub_edit");
            edits[1]->setText("Videos");
        }
        dlg.show();
        pause(400);
        c->check("Dialog Discord: Grenze 10 MB angezeigt", dlg.estimateText().contains("10 MB") && dlg.twoPass(), dlg.estimateText());
        dlg.grab().save(QDir(dir).filePath("export-discord.png"));
        dlg.selectPreset("tiktok");
        dlg.setFit(2);
        pause(400);
        c->check("Dialog TikTok: 1080 × 1920 · 9:16", dlg.estimateText().contains("1080 × 1920 · 9:16"), dlg.estimateText());
        c->check("Dialog TikTok: Optionen = Vorlage", dlg.options().finalW() == 1080 && dlg.options().finalH() == 1920 && dlg.options().fit == 2);
        dlg.grab().save(QDir(dir).filePath("export-tiktok.png"));
        dlg.selectPreset("yt4k");
        pause(200);
        c->check("Dialog 4K: Hinweis hochskaliert", dlg.estimateText().contains(T("pr_upscaled")));
        dlg.selectPreset(QString());
        c->check("Eigene Einstellungen: wie bisher", dlg.options().outW == 0 && dlg.options().videoKbps == 0);
        dlg.hide();
    }
    c->finish();
}

// ---------------------------------------------------------------- Untertitel
void MainWindow::subtitleTest(const QString& dir, bool online) {
    silent_ = true;
    audio_->setMuted(true);
    QDir().mkpath(dir);
    auto* c = new Checker{dir, "untertitel"};
    QTemporaryDir tmp;

    // Zeiten
    c->check("Zeit 0 (SRT)", subTime(0, false) == "00:00:00,000");
    c->check("Zeit 1:01:01,5", subTime(3661.5, false) == "01:01:01,500");
    c->check("Rundung mit Übertrag", subTime(59.9996, false) == "00:01:00,000", subTime(59.9996, false));
    c->check("VTT mit Punkt", subTime(12.345, true) == "00:00:12.345");
    c->check("Negativ -> 0", subTime(-3, false) == "00:00:00,000");
    c->check("Lesen: SRT, VTT, ohne Stunden", near(parseSubTime("00:01:02,345"), 62.345, 1e-9) &&
                                                   near(parseSubTime("01:02.5"), 62.5, 1e-9) && near(parseSubTime("1:00:00.000"), 3600, 1e-9));
    c->check("Lesen: Unsinn", parseSubTime("12:99:00") < 0 && parseSubTime("abc") < 0);

    // SRT/VTT
    QList<SubCue> cues = {{0.5, 2.0, "Hallo Welt"}, {2.5, 4.25, "Zweite Zeile\nmit Umbruch"}, {3661.0, 3662.5, "Spät"}};
    const QString srt = toSrt(cues);
    c->check("SRT-Format", srt.startsWith("1\n00:00:00,500 --> 00:00:02,000\nHallo Welt\n\n2\n"), srt.left(60));
    const QList<SubCue> back = parseSubtitles(srt);
    c->check("SRT hin und zurück", back.size() == 3 && back[1].text == "Zweite Zeile\nmit Umbruch" && near(back[2].t0, 3661.0, 1e-6));
    const QString vtt = toVtt(cues);
    c->check("VTT-Kopf und Punkt", vtt.startsWith("WEBVTT\n\n00:00:00.500 --> 00:00:02.000\n"));
    c->check("VTT hin und zurück", parseSubtitles(vtt).size() == 3);
    const QString messy = QString(QChar(0xfeff)) + "WEBVTT\r\n\r\nNOTE Kommentar\r\n\r\nid1\r\n00:01.000 --> 00:02.500 align:center\r\n"
                                                    "<i>kursiv</i> und <c.gelb>farbig</c>\r\n\r\n00:03.000 --> 00:04.000\r\nletzte\r\n";
    const QList<SubCue> m = parseSubtitles(messy);
    c->check("VTT mit BOM, CRLF, NOTE, Kennung, Tags", m.size() == 2 && m[0].text == "kursiv und farbig" && near(m[0].t1, 2.5, 1e-6),
             m.isEmpty() ? QString() : m[0].text);
    QList<SubCue> norm = {{5, 6, "c"}, {1, 3, "a"}, {2, 4, "b"}, {7, 7.5, "  "}, {9, 12, "zu lang"}, {11, 13, "danach"}};
    normalizeCues(norm, 10);
    c->check("Aufräumen: sortiert, leere und zu späte weg", norm.size() == 4 && norm[0].text == "a" && norm.last().text == "zu lang",
             QString::number(norm.size()));
    c->check("Aufräumen: Überschneidung aufgelöst", norm.size() >= 2 && norm[0].t1 <= norm[1].t0 + 1e-9);
    c->check("Aufräumen: auf Länge begrenzt", !norm.isEmpty() && near(norm.last().t1, 10, 1e-9));
    c->check("Untertitel zur Zeit", cueAt(cues, 1.0) == 0 && cueAt(cues, 2.2) == -1 && cueAt(cues, 3.0) == 1);

    // whisper.cpp-Ausgabe
    const QByteArray json = R"({"result":{"language":"de"},"transcription":[
        {"timestamps":{"from":"00:00:00,000","to":"00:00:01,200"},"offsets":{"from":0,"to":1200},"text":" Guten Tag"},
        {"offsets":{"from":1200,"to":2000},"text":" [BLANK_AUDIO]"},
        {"timestamps":{"from":"00:00:02,000","to":"00:00:03,500"},"text":" zusammen."}]})";
    QString lang;
    const QList<SubCue> wj = parseWhisperJson(json, &lang);
    c->check("whisper-JSON: Text, Zeiten, Sprache", wj.size() == 2 && wj[0].text == "Guten Tag" && near(wj[1].t1, 3.5, 1e-6) && lang == "de");
    const QStringList wa = whisperArgs("/m/ggml-base.bin", "/a.wav", "auto", false, "/o/r", 4);
    c->check("whisper-Aufruf: Sprache auto, JSON", wa.contains("-oj") && wa.join(' ').contains("-l auto") && !wa.contains("-tr"));
    c->check("whisper-Aufruf: übersetzen", whisperArgs("m", "a", "de", true, "o", 2).contains("-tr"));
    c->check("Modelle aus offizieller Quelle", whisperModelUrl("base") == "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin");
    c->check("Programm für dieses System", !whisperProgramAsset().isEmpty() && whisperProgramUrls().size() >= 1, whisperProgramAsset());
    c->check("Plugin in den Einstellungen", [] {
        for (const PluginInfo& p : plugins()) if (QString(p.id) == "subtitles") return true;
        return false;
    }());

    // Einbrennen: Bilder + Liste
    SubStyle st;
    const QString burnDir = tmp.filePath("burn");
    const QString list = writeSubtitleBurn({{1.0, 2.0, "TEST"}, {2.5, 2.9, "zwei"}}, st, QSize(640, 360), 3.0, burnDir);
    QFile lf(list);
    lf.open(QIODevice::ReadOnly);
    const QString lt = QString::fromUtf8(lf.readAll());
    double sum = 0;
    for (const auto& mm : QRegularExpression(R"(duration ([\d.]+))").globalMatch(lt)) sum += mm.captured(1).toDouble();
    c->check("Einbrennen: Liste mit Lücken und Untertiteln", lt.startsWith("ffconcat version 1.0") && lt.count("file 'c0001.png'") == 1 &&
                                                              near(sum, 4.0, 0.01), QString("Summe %1").arg(sum));
    const QImage cue1(QDir(burnDir).filePath("c0001.png"));
    c->check("Einbrennen: Bild in Zielgröße, Text unten", cue1.size() == QSize(640, 360) &&
                                                          qAlpha(cue1.pixel(320, 50)) == 0 && [&] {
                                                              for (int y = 250; y < 345; ++y)
                                                                  for (int x = 200; x < 440; ++x) if (qAlpha(cue1.pixel(x, y)) > 200) return true;
                                                              return false;
                                                          }());
    {
        Project pr;
        Source s;
        s.path = "/in/a.mp4";
        s.duration = 3;
        s.hasAudio = true;
        pr.sources << s;
        pr.pieces << Piece{0, 0, 3, 1, 1};
        ExportOptions o;
        o.width = 640;
        o.height = 360;
        o.subBurn = "/tmp/subs.ffconcat";
        const QString cmd = buildExport(pr, o, "/out.mp4").join(' ');
        c->check("Einbrennen-Filter", cmd.contains("-f concat -safe 0 -i /tmp/subs.ffconcat") &&
                                          cmd.contains("[1:v]format=rgba[subs];[vc][subs]overlay=0:0:eof_action=repeat[sb]") &&
                                          cmd.contains("[sb]format=yuv420p[vout]"));
        o.subBurn.clear();
        o.subFile = "/tmp/s.srt";
        const QString soft = buildExport(pr, o, "/out.mp4").join(' ');
        c->check("Untertitelspur-Befehl (mov_text)", soft.contains("-map 1:0") && soft.contains("-c:s mov_text"));
    }

    // Echt: einbrennen und als Spur einbetten
    const QString src = tmp.filePath("Beispiel.mp4");
    makeVideo(src, "color=c=0x303030", 640, 360, 3, 440);
    openFile(src);
    c->check("Video geladen", waitFor([this] { return !pr_.pieces.isEmpty() && canvasSet_; }, 20000));
    ensureProbed();
    pr_.subs = {{1.0, 2.0, "TEST"}};
    {
        ExportOptions o;
        o.width = 640;
        o.height = 360;
        o.fps = 30;
        const QString out = tmp.filePath("eingebrannt.mp4");
        QString err;
        const bool ok = runExportBlocking(prepareExport(pr_, o, out, false, 1), &err);
        const QImage a = frameAt(out, 0.5, tmp.path()), b = frameAt(out, 1.5, tmp.path()), d = frameAt(out, 2.5, tmp.path());
        const QRect area(150, 240, 340, 110);
        c->check("Eingebrannt: Export", ok, err.left(300));
        c->check("Eingebrannt: weißer Text nur zwischen 1 und 2 s", !a.isNull() && !b.isNull() && !d.isNull() &&
                                                                       maxLuma(b, area) > 200 && maxLuma(a, area) < 70 && maxLuma(d, area) < 70,
                 QString("hellster Punkt %1 / %2 / %3").arg(maxLuma(a, area)).arg(maxLuma(b, area)).arg(maxLuma(d, area)));
        o.outW = 360;  // Vorlage 9:16: Untertitel sitzen im Zielbild
        o.outH = 640;
        o.width = 360;
        o.height = 202;
        const QString out2 = tmp.filePath("eingebrannt-916.mp4");
        const bool ok2 = runExportBlocking(prepareExport(pr_, o, out2, false, 1), &err);
        const MediaInfo m2 = probeMedia(out2);
        c->check("Eingebrannt bei 9:16-Ziel", ok2 && m2.w == 360 && m2.h == 640, QString("%1×%2").arg(m2.w).arg(m2.h));
        ExportOptions o3;
        o3.width = 640;
        o3.height = 360;
        const QString out3 = tmp.filePath("spur.mp4");
        const bool ok3 = runExportBlocking(prepareExport(pr_, o3, out3, false, 2), &err);
        c->check("Als Untertitelspur in MP4", ok3 && streams(out3).contains("mov_text"), err.left(200));
        o3.format = "mkv";
        const QString out4 = tmp.filePath("spur.mkv");
        const bool ok4 = runExportBlocking(prepareExport(pr_, o3, out4, false, 2), &err);
        c->check("Als Untertitelspur in MKV", ok4 && streams(out4).contains("Subtitle: subrip"), err.left(200));
    }

    // Editor: Spur, Vorschau, Bearbeiten, Rückgängig
    QSettings().setValue("plugins/subtitles", true);
    applyLayoutVisibility();
    btnEdit_->setChecked(true);
    resize(1400, 900);
    pr_.subs.clear();
    modelEdited();
    seek(0.3);
    addSubtitleAtPlayhead();
    c->check("Untertitel an der Abspielposition", pr_.subs.size() == 1 && selSub_ == 0 && near(pr_.subs[0].t0, 0.3, 0.05));
    subEdit_->setPlainText("Willkommen bei Vidline");
    c->check("Text bearbeiten", pr_.subs[0].text == "Willkommen bei Vidline");
    bool hasRow = false;
    for (const auto& r : timeline_->rows()) hasRow |= r.kind == Timeline::Row::Subs;
    c->check("Eigene Zeile in der Zeitleiste", hasRow);
    seek(1.0);
    c->check("Vorschau zeigt den Untertitel", subItem_ && !subItem_->pixmap().isNull());
    seek(2.95);
    c->check("Vorschau leer außerhalb", subItem_ && subItem_->pixmap().isNull());
    seek(1.5);
    selectSub(0);
    doSplit();
    c->check("Untertitel teilen", pr_.subs.size() == 2 && near(pr_.subs[0].t1, 1.5, 0.01) && near(pr_.subs[1].t0, 1.5, 0.01));
    pr_.subs[1].text = "mit automatischen Untertiteln";
    editSubStyle([](SubStyle& s) { s.look = 1; s.size = 60; });
    c->check("Aussehen ändern", pr_.subStyle.look == 1 && pr_.subStyle.size == 60);
    doUndo();
    c->check("Rückgängig: Aussehen", pr_.subStyle.look == 0);
    selectSub(1);
    deleteSelected();
    c->check("Untertitel löschen", pr_.subs.size() == 1);
    doUndo();
    c->check("Rückgängig: wieder da", pr_.subs.size() == 2);
    c->check("Werkzeug-Knopf sichtbar", btnSubs_ && btnSubs_->isVisibleTo(this));
    seek(0.8);
    selectSub(0);
    pause(800);
    grab().save(QDir(dir).filePath("untertitel-editor.png"));
    if (qEnvironmentVariableIsSet("VIDLINE_DEBUG_INSP"))
        for (int i = 0; i < insp_->count(); ++i) {
            QWidget* pg = insp_->widget(i);
            QString wide;
            for (QWidget* w : pg->findChildren<QWidget*>())
                if (w->minimumSizeHint().width() > 200) wide += QString("%1:%2 ").arg(w->metaObject()->className()).arg(w->minimumSizeHint().width());
            c->note(QString("Seite %1: %2 px, Bereich %3 px – %4").arg(i).arg(pg->minimumSizeHint().width()).arg(insp_->width()).arg(wide));
        }
    inspCard_->grab().save(QDir(dir).filePath("untertitel-eigenschaften.png"));
    {
        SubtitleDialog dlg(pr_, this);
        dlg.show();
        pause(300);
        dlg.grab().save(QDir(dir).filePath("untertitel-dialog.png"));
    }
    {
        PluginDialog dlg(this);
        dlg.show();
        pause(300);
        dlg.grab().save(QDir(dir).filePath("plugins.png"));
    }
    // Vorschau-Bild des Untertitels allein (für die Webseite)
    renderSubtitleFrame("Untertitel automatisch erkannt", SubStyle{}, QSize(1280, 720)).save(QDir(dir).filePath("untertitel-bild.png"));

    // Mit Netz: echte Erkennung eines kurzen gesprochenen Satzes (Stimme von ffmpeg/flite)
    if (!online) {
        c->note("Ohne Netz: echte Erkennung übersprungen (Aufruf mit --online)");
        return c->finish();
    }
    const QString speech = tmp.filePath("satz.mp4");
    QString err;
    const bool tts = ff({"-f", "lavfi", "-i", "flite=text='Hello world. This is a short test of automatic subtitles.':voice=slt",
                         "-f", "lavfi", "-i", "color=c=0x303030:s=640x360:r=30", "-shortest", "-c:v", "libx264", "-preset",
                         "ultrafast", "-pix_fmt", "yuv420p", "-c:a", "aac", "-ar", "48000", speech}, &err);
    if (!tts) {
        c->note("ffmpeg ohne flite: kein gesprochener Testsatz möglich (" + err.left(120) + ")");
        return c->finish();
    }
    openFile(speech);
    waitFor([this] { return !pr_.pieces.isEmpty() && canvasSet_; }, 20000);
    ensureProbed();
    WhisperJob job(pr_, "tiny", "en", false);
    QStringList states;
    connect(&job, &WhisperJob::progress, this, [&states](const QString& s, double) { if (states.isEmpty() || states.last() != s) states << s; });
    bool done = false, ok = false;
    QString jobErr;
    connect(&job, &WhisperJob::finished, this, [&](bool o, const QString& e) { done = true; ok = o; jobErr = e; });
    QElapsedTimer t;
    t.start();
    job.start();
    waitFor([&] { return done; }, 15 * 60000);
    QString text;
    for (const SubCue& s : job.cues()) text += s.text + " ";
    c->check("Echte Erkennung: fertig", done && ok, jobErr.left(300));
    c->check("Echte Erkennung: Satz verstanden", text.contains("test", Qt::CaseInsensitive) && text.contains("subtitle", Qt::CaseInsensitive),
             text.trimmed());
    c->check("Echte Erkennung: Zeiten innerhalb des Videos", !job.cues().isEmpty() && job.cues().last().t1 <= pr_.total() + 0.01);
    c->note(QString("Dauer %1 s, Schritte: %2").arg(t.elapsed() / 1000.0, 0, 'f', 1).arg(states.filter(QRegularExpression("^(?!.*\\d+ %)")).join(" | ")));
    c->finish();
}
