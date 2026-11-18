#pragma once
#include <QList>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <cstdlib>

struct Source {
    QString path;
    double duration = 0;
    double fps = 0;
    bool hasAudio = false;
    bool video = true;  // false: reine Tondatei (MP3, WAV …) - Bild bleibt schwarz
    bool probed = false;
};

struct Piece {
    int src = 0;
    double start = 0, end = 0, speed = 1.0;
    double volume = 1.0;  // Lautstärke des Video-Tons in diesem Abschnitt (0..6 = bis 600 %)
    bool denoise = false;  // Rauschunterdrückung (Klicks, Rauschen) beim Export
    // Bild anpassen: Größe (1 = eingepasst), Verschiebung als Anteil der Bildfläche, Drehung in Grad
    double scale = 1.0, px = 0, py = 0, rot = 0;
    double outDur() const { return (end - start) / speed; }
    bool transformed() const {
        return std::abs(scale - 1) > 1e-4 || std::abs(px) > 1e-4 || std::abs(py) > 1e-4 || std::abs(rot) > 1e-3;
    }
};

struct Item {
    // Video: Bild-im-Bild/Overlay auf einer Video-Spur (V2, V3 …) mit eigenem Ton
    enum Kind { Image, Blur, Text, Draw, Shape, Video };
    enum ShapeKind { Rect, Ellipse, Arrow, Line };
    int id = 0;
    Kind kind = Blur;
    double t0 = 0, t1 = 0;              // Ausgabezeit
    double x = 0, y = 0, w = 0, h = 0;  // normalisiert (0..1) relativ zum Bild
    QString path;
    double strength = 30.0;             // Blur-Sigma (bei 1080p)
    double ar = 1.0;                    // Seitenverhältnis h/w (Bild)
    // Text: Inhalt, Schriftart, Farbe (ARGB), optional Hintergrund
    QString text, font;
    unsigned color = 0xffffffffu, bgColor = 0xcc000000u;
    bool bg = false;
    // Zeichnung / Form: Linien in Koordinaten des Rahmens (0..1), Strichstärke in Pixeln bei 1080p.
    // Pfeil und Linie: strokes[0] = Start- und Endpunkt.
    int shape = Rect;
    double stroke = 8.0;
    bool fill = false;
    QList<QPolygonF> strokes;
    // Spur: 0 = eigene Zeile wie bisher (Text, Unschärfe, Zeichnung …), sonst ID einer Video-Spur (Bild/Video)
    int track = 0;
    double opacity = 1.0;              // Deckkraft (Bild und Video)
    double srcStart = 0, fileDur = 0;  // Video: Startpunkt in der Datei, Länge der Datei
    double volume = 1.0;               // Video: Lautstärke des Tons
    bool hasAudio = false;             // Video: Datei hat Ton
    bool graphic() const { return kind == Draw || kind == Shape; }
    bool media() const { return kind == Image || kind == Video; }
};

struct AudioClip {
    int id = 0;
    QString path;
    double t0 = 0;        // Startzeit in der Ausgabe
    double srcStart = 0;  // Startpunkt in der Datei
    double dur = 0;       // Länge in der Ausgabe
    double fileDur = 0;
    double volume = 1.0;
    bool denoise = false;  // Rauschunterdrückung beim Export
    int track = 0;         // ID der Ton-Spur (0 = erste Ton-Spur)
};

// Spuren: V1 ist das Hauptvideo (die Abschnitte), weitere Video-Spuren liegen darüber (Bild-im-Bild, Overlay).
// Ton-Spuren (A1, A2 …) für Musik, Sprache usw. Jede Spur hat Lautstärke, Stumm und Solo (wirkt auf ihren Ton).
struct Track {
    int id = 0;
    bool audio = false;
    double volume = 1.0;
    bool mute = false, solo = false;
};
constexpr int kMainTrack = 1;

// Untertitel: Text mit Zeiten in der Ausgabe
struct SubCue {
    double t0 = 0, t1 = 0;
    QString text;
};
struct SubStyle {
    QString font;                       // leer = Standardschrift
    double size = 54;                   // Schriftgröße in Pixeln bei 1080 Pixeln (kurze Seite)
    int pos = 0;                        // 0 unten, 1 Mitte, 2 oben
    double margin = 0.07;               // Abstand zum Rand (Anteil der Höhe)
    int look = 0;                       // 0 Rand (Kontur), 1 Hintergrund-Kasten, 2 nur Schatten
    unsigned color = 0xffffffffu, edgeColor = 0xff000000u, boxColor = 0xb4000000u;
    bool bold = true;
};

struct Snapshot {
    QList<Piece> pieces;
    QList<Item> items;
    QList<AudioClip> audios;
    int aspect = 0;
    QList<Track> tracks;
    QList<SubCue> subs;
    SubStyle subStyle;
};

// Bildformate der Ausgabe; w == 0 heißt "wie das erste Video"
struct AspectFormat { int w, h; const char* label; };
constexpr int kAspectCount = 7;
extern const AspectFormat kAspects[kAspectCount];
// Größe der Bildfläche: die kurze Seite des Originals bleibt (z. B. 1920×1080 -> 9:16 = 1080×1920)
QSize canvasSize(int aspect, int natW, int natH);

class Project {
public:
    QList<Source> sources;
    QList<Piece> pieces;
    QList<Item> items;
    QList<AudioClip> audios;
    QList<Track> tracks = {Track{kMainTrack, false}, Track{2, true}};  // V1 (Hauptvideo) und A1
    QList<SubCue> subs;
    SubStyle subStyle;
    int vw = 1920, vh = 1080;  // Bildfläche (aus Bildformat und Größe des ersten Videos)
    int natW = 0, natH = 0;    // Größe des ersten Videos
    int aspect = 0;            // Index in kAspects
    int nextId = 3;            // gemeinsame IDs für Elemente, Ton-Clips und Spuren

    // ---- Spuren
    Track* track(int id) {
        for (Track& t : tracks) if (t.id == id) return &t;
        return nullptr;
    }
    const Track* track(int id) const {
        for (const Track& t : tracks) if (t.id == id) return &t;
        return nullptr;
    }
    QList<int> trackIds(bool audio) const {
        QList<int> r;
        for (const Track& t : tracks) if (t.audio == audio) r << t.id;
        return r;
    }
    // Position innerhalb der Video- bzw. Ton-Spuren (V1 = 0), -1 wenn es die Spur nicht gibt
    int trackIndex(int id) const {
        const Track* t = track(id);
        return t ? int(trackIds(t->audio).indexOf(id)) : -1;
    }
    QString trackName(int id) const {
        const Track* t = track(id);
        return t ? QString("%1%2").arg(t->audio ? 'A' : 'V').arg(trackIndex(id) + 1) : QString();
    }
    int addTrack(bool audio) {
        Track t;
        t.id = nextId++;
        t.audio = audio;
        tracks.append(t);
        return t.id;
    }
    // Ton-Spur eines Clips (alte Clips ohne Spur gehören zur ersten Ton-Spur)
    int audioTrackOf(const AudioClip& a) const {
        if (const Track* t = track(a.track); t && t->audio) return a.track;
        const QList<int> ids = trackIds(true);
        return ids.isEmpty() ? 0 : ids.first();
    }
    // Wirksame Lautstärke einer Spur: stumm = 0, bei Solo irgendwo nur die Solo-Spuren
    double trackGain(int id) const {
        bool anySolo = false;
        for (const Track& t : tracks) anySolo |= t.solo;
        const Track* t = track(id);
        if (!t) return anySolo ? 0.0 : 1.0;
        if (t->mute || (anySolo && !t->solo)) return 0.0;
        return t->volume;
    }
    // Overlay-Clips einer Video-Spur (Bild/Video), nach Zeit sortiert
    QList<const Item*> trackClips(int id) const {
        QList<const Item*> r;
        for (const Item& it : items) if (it.track == id && id != 0) r << &it;
        std::sort(r.begin(), r.end(), [](const Item* a, const Item* b) { return a->t0 < b->t0; });
        return r;
    }

    double total() const {
        double t = 0;
        for (const Piece& p : pieces) t += p.outDur();
        return t;
    }
    double outStart(int i) const {
        double t = 0;
        for (int k = 0; k < i && k < pieces.size(); ++k) t += pieces[k].outDur();
        return t;
    }
    // Ausgabezeit -> Abschnitt-Index + Quellzeit
    int locate(double t, double* src) const {
        double acc = 0;
        for (int i = 0; i < pieces.size(); ++i) {
            const Piece& p = pieces[i];
            if (t < acc + p.outDur() || i == pieces.size() - 1) {
                if (src) *src = p.start + std::clamp(t - acc, 0.0, p.outDur()) * p.speed;
                return i;
            }
            acc += p.outDur();
        }
        if (src) *src = 0;
        return 0;
    }
    double srcDuration(int i) const { return sources.value(pieces.value(i).src).duration; }
    Item* item(int id) {
        for (Item& it : items) if (it.id == id) return &it;
        return nullptr;
    }
    AudioClip* audio(int id) {
        for (AudioClip& a : audios) if (a.id == id) return &a;
        return nullptr;
    }
    Snapshot snapshot() const { return {pieces, items, audios, aspect, tracks, subs, subStyle}; }
    void restore(const Snapshot& s) {
        pieces = s.pieces; items = s.items; audios = s.audios; aspect = s.aspect;
        tracks = s.tracks; subs = s.subs; subStyle = s.subStyle;
    }
};

QString fmtTime(double t);

struct MediaInfo {
    bool ok = false;
    double duration = 0, fps = 0;
    bool hasAudio = false;
    int w = 0, h = 0;
};

// ---- ffmpeg
QString ffmpegPath();
// RNNoise-Modell für ffmpegs arnndn (aus den Ressourcen in den Cache entpackt)
QString denoiseModelPath();
QString detectEncoder();
MediaInfo probeMedia(const QString& path);

struct ExportOptions {
    int width = 1920, height = 1080;  // Bildfläche, auf der alles zusammengesetzt wird (Seitenverhältnis des Projekts)
    double fps = 30;
    int quality = 1;  // 0 hoch, 1 ausgewogen, 2 klein
    QString encoder = "libx264";
    QString format = "mp4";  // mp4 mov mkv gif | mp3 wav m4a (nur Ton)
    // Zielgröße (Vorlagen wie 9:16 oder 1:1): 0 = wie width/height. Passt das Seitenverhältnis nicht,
    // entscheidet fit: 0 schwarzer Rand (Polster), 1 zuschneiden, 2 unscharfer Rand
    int outW = 0, outH = 0;
    int fit = 0;
    int videoKbps = 0;    // > 0: feste Bitrate statt Qualitätsstufe (für eine Ziel-Dateigröße)
    int audioKbps = 0;    // 0 = Standard (192 kbit/s bei Video)
    int pass = 0;         // 2-Pass: 1 = erster Durchgang (nur Analyse), 2 = zweiter; 0 = normal
    QString passLog;      // Datei-Präfix für die 2-Pass-Statistik
    int sampleRate = 0;   // nur Ton: Abtastrate (0 = 48 kHz)
    bool mono = false;    // nur Ton: Mono (für die Spracherkennung)
    int gifWidth = 640;   // GIF: höchstens so breit
    double gifFps = 15;
    QString subBurn;      // Untertitel einbrennen: ffconcat-Liste mit den Untertitel-Bildern (leer = nein)
    QString subFile;      // Untertitel als eigene Spur einbetten (SRT-Datei, leer = nein)
    int finalW() const { return outW > 0 ? outW : width; }
    int finalH() const { return outH > 0 ? outH : height; }
};

bool isAudioFormat(const QString& format);

QStringList buildExport(const Project& pr, const ExportOptions& o, const QString& out);
QStringList encArgs(const QString& encoder, int quality);  // Video-Encoder-Argumente je Qualitätsstufe
