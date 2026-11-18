#pragma once
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <memory>
#include "model.h"

// Export-Vorlagen: fertige Einstellungen für TikTok, YouTube, Discord … und eigene, gespeicherte Vorlagen.
struct ExportPreset {
    QString id;            // "tiktok", "yt1080" … bzw. "custom:<Name>"
    QString name;          // eigene Vorlagen: Name; eingebaute: Übersetzungsschlüssel in key
    const char* key = nullptr;
    int w = 0, h = 0;      // Zielgröße, 0 = wie das Projekt
    int fit = 0;           // 0 schwarzer Rand, 1 zuschneiden, 2 unscharfer Rand
    double maxFps = 60;    // höchstens diese Bildrate (Original, falls kleiner)
    QString format = "mp4";
    int quality = 1;
    double maxMB = 0;      // Ziel-Dateigröße in MB (1 MB = 1 000 000 Bytes), 0 = keine
    int audioKbps = 0;
    int maxShort = 0;      // höchstens diese kurze Seite (0 = keine Grenze), z. B. Discord
    int gifWidth = 640;
    double gifFps = 15;
    bool builtin = true;
    QString title() const;
};

QList<ExportPreset> builtinPresets();
QList<ExportPreset> customPresets();
QList<ExportPreset> allPresets();
const ExportPreset* findPreset(const QList<ExportPreset>& list, const QString& id);
void saveCustomPreset(const ExportPreset& p);
void removeCustomPreset(const QString& name);
QString presetToJson(const ExportPreset& p);
ExportPreset presetFromJson(const QString& json);

// Was beim Export mit einer Vorlage herauskommt
struct ExportPlan {
    ExportOptions o;
    bool twoPass = false;
    double maxBytes = 0;   // Grenze der Dateigröße (0 = keine)
    bool tooLong = false;  // für die Zielgröße zu lang: Bitrate am unteren Rand
    QString note;
};
// Projekt: Bildfläche cw×ch, Bildrate des Originals, Länge, Ton ja/nein
ExportPlan planPreset(const ExportPreset& p, int cw, int ch, double srcFps, double duration, bool hasAudio);
// Bitrate (kbit/s) fürs Bild, damit die Datei unter maxBytes bleibt
int sizeTargetVideoKbps(double maxBytes, double duration, int audioKbps);
// Bildfläche für eine Zielgröße: Seitenverhältnis des Projekts, passend (Rand) oder füllend (Zuschnitt)
QSize compositionSize(int cw, int ch, int tw, int th, int fit);

// Export vorbereiten: Texte/Zeichnungen und Untertitel als Bilder rendern, Befehle (1 oder 2 Durchgänge)
// subMode: 0 keine Untertitel, 1 einbrennen, 2 als Untertitelspur einbetten
struct ExportJob {
    QList<QStringList> passes;
    std::shared_ptr<class QTemporaryDir> tmp;
};
ExportJob prepareExport(const Project& pr, ExportOptions eo, const QString& out, bool twoPass, int subMode);
// Für Tests: alle Durchgänge nacheinander ausführen
bool runExportBlocking(const ExportJob& job, QString* err, int timeoutMs = 600000);
