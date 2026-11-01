#pragma once
#include <QImage>
#include <QString>

// Angaben einer Tondatei. cover ist das eingebettete Bild, leer heißt: keins.
struct AudioTags {
    QString title, artist, album, albumArtist, year, track, genre;
    QImage cover;
    bool coverChanged = false;  // beim Schreiben: cover einbetten, oder das vorhandene entfernen, wenn es leer ist
};

// Nicht jedes Format kann ein Bild speichern
bool canEmbedCover(const QString& path);
AudioTags readAudioTags(const QString& path);
// Schreibt in eine neue Datei daneben und tauscht sie erst dann gegen das Original, der Ton wird nicht neu kodiert.
// Die Datei darf dabei nicht geöffnet sein. Geht etwas schief, bleibt das Original unverändert.
bool writeAudioTags(const QString& path, const AudioTags& tags, QString* error);
