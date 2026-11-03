#pragma once
#include <QDialog>
#include <QList>
#include <QString>
#include "icons.h"

// Plugins: Zusatzfunktionen, die man in den Einstellungen dazuholt. Der Code steckt in Vidline, das Plugin schaltet
// ihn frei; was es zusätzlich braucht (beim Musik-Plugin yt-dlp), lädt es erst, wenn man es benutzt.
struct PluginInfo {
    const char* id;
    const char* nameKey;
    const char* descKey;
    Ic icon;
};

const QList<PluginInfo>& plugins();
bool pluginEnabled(const char* id);
void setPluginEnabled(const char* id, bool on);

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
