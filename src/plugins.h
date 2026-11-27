#pragma once
#include <QDialog>
#include <QList>
#include <QString>
#include "icons.h"

// Plugins: Zusatzfunktionen, die man in den Einstellungen dazuholt. Der Code steckt in Vidline, das Plugin schaltet
// ihn frei; was es zusätzlich braucht (beim Video-Download yt-dlp, bei Untertiteln Whisper), lädt es erst, wenn man es benutzt.
// Das frühere Musik-Plugin ist seit 1.18.0 ein eigenes Programm: Tuneline.
struct PluginInfo {
    const char* id;
    const char* nameKey;
    const char* descKey;
    Ic icon;
};

const QList<PluginInfo>& plugins();
bool pluginEnabled(const char* id);
void setPluginEnabled(const char* id, bool on);

QString pluginDataDir();  // Datenordner der Plugins (VIDLINE_DATA für Tests)
QString ytDlpPath();      // yt-dlp für den Video-Download

// Wer das alte Musik-Plugin an hatte, bekommt einmal den Hinweis auf Tuneline (und die Musik bleibt dort erhalten)
void showTunelineHint(QWidget* parent);

class QVBoxLayout;

class PluginDialog : public QDialog {
    Q_OBJECT
public:
    explicit PluginDialog(QWidget* parent = nullptr);

signals:
    void changed();

private:
    void rebuild();
    QVBoxLayout* list_;
};
