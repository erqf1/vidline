#pragma once
#include <QKeySequence>
#include <QList>
#include <QString>

// Alle Tastenkürzel von Vidline an einer Stelle: Kennung, Beschriftung (Übersetzungsschlüssel), Standard.
// Eigene Belegungen liegen in QSettings unter "shortcuts/<id>" (leer = keine Taste).
struct ShortcutDef {
    const char* id;
    const char* label;
    const char* def;
};

const QList<ShortcutDef>& shortcutDefs();
QKeySequence shortcutFor(const QString& id);
QKeySequence defaultShortcut(const QString& id);
void setShortcut(const QString& id, const QKeySequence& seq);
void resetShortcuts();
QString shortcutText(const QString& id);  // für Tooltips, z. B. "Strg+O"; leer = keine Taste
