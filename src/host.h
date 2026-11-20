#pragma once
#include <QImage>
#include <QRectF>
#include <vector>
#include "model.h"

// Vorschaubilder einer Videodatei für die Timeline
struct ThumbSet {
    double step = 1.0;
    QList<QImage> imgs;
};

// Lautstärke-Verlauf einer Datei (Spitzenwerte 0..1, "rate" Werte pro Sekunde)
struct WaveSet {
    double rate = 50;
    std::vector<float> peaks;
};

// Schnittstelle, über die Timeline und Overlays mit dem Hauptfenster reden.
class EditorHost {
public:
    virtual ~EditorHost() = default;
    virtual Project& pr() = 0;
    virtual double curTime() const = 0;
    virtual int selPiece() const = 0;
    virtual int selItem() const = 0;   // Item-ID, 0 = keins
    virtual int selAudio() const = 0;  // Ton-ID, 0 = keins
    virtual void selectPiece(int i) = 0;
    virtual void selectItem(int id) = 0;
    virtual void selectAudio(int id) = 0;
    virtual void pushUndo() = 0;
    virtual void modelEdited(bool keepTime = false) = 0;
    virtual void seek(double t) = 0;
    virtual void scrub(double t) = 0;  // beim Ziehen: Anzeige sofort, Player gedrosselt
    // Spulen per Ziehen: solange gezogen wird, pausiert und stumm (sonst kommen Tonfetzen), danach weiter wie vorher
    virtual void beginScrub() = 0;
    virtual void endScrub() = 0;
    virtual double viewScale() const = 0;
    virtual int timelineViewportWidth() const = 0;
    virtual int timelineViewportHeight() const = 0;
    virtual const ThumbSet* thumbs(int srcIndex) const = 0;
    virtual const WaveSet* waves(const QString& path) const = 0;
    virtual void timelineZoomed() = 0;  // nach dem Zoomen: Abspielposition sichtbar halten
    virtual QImage frameImage() = 0;   // aktuelles Videobild (für echte Unschärfe in der Vorschau)
    virtual QRectF videoRect() const = 0;  // Bereich des Videos in Szenenkoordinaten
    virtual bool showGuides() const = 0;   // Rahmen/Griffe anzeigen (nicht im Vollbild)
    virtual void insertMediaAt(const QString& path, double t) = 0;  // Datei an Stelle t einfügen (Drag & Drop)
    // Spuren und Untertitel
    virtual int selSub() const = 0;    // Index des ausgewählten Untertitels, -1 = keiner
    virtual void selectSub(int i) = 0;
    virtual int selTrack() const = 0;  // ausgewählte Spur (Kopf angeklickt), 0 = keine
    virtual void selectTrack(int id) = 0;
    // Clip auf eine andere Spur legen: kind 'p' Abschnitt (id = Index), 'i' Element, 'a' Ton; track -1 = neue Spur
    virtual bool moveToTrack(char kind, int id, int track) = 0;
    virtual void insertMediaOnTrack(const QString& path, double t, int track) = 0;
    virtual void tracksChanged() = 0;  // Stumm/Solo/Lautstärke einer Spur geändert
    virtual void overlayMoved() {}     // Element in der Vorschau verschoben/vergrößert (Overlay-Video zieht mit)
};
