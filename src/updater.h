#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QNetworkAccessManager;
class QWidget;

// Texte kommen aus der jeweiligen App (eigene Übersetzungstabelle)
struct UpdaterTexts {
    QString title;        // "Update available"
    QString text;         // "%1 %2 is available (you have %3)."  (%1 App, %2 neu, %3 installiert)
    QString now, ignore, later;
    QString downloading;  // "Downloading update…"
    QString failed;       // "Couldn't install automatically – the download page opens."
    QString upToDate;     // "You're using the latest version."
    QString cancel;
};

// Sucht auf GitHub nach einer neueren Version, fragt nach und installiert sie auf allen Plattformen:
// Windows: Installer (still) oder ZIP über den portablen Ordner · macOS: .app aus der passenden DMG ersetzen ·
// Linux: .deb (apt) / .pkg.tar.zst (pacman) über pkexec oder tar.gz über den portablen Ordner.
// Ein kleines Hilfsskript wartet, bis die App beendet ist, installiert und startet sie neu.
class Updater : public QObject {
    Q_OBJECT
public:
    struct Options {
        QString repo;                      // "erqf1/vidline"
        QString appName;                   // "Vidline"
        QString version;                   // installierte Version, z. B. "1.3.0"
        QStringList relaunchArgs;          // Argumente für den Neustart
        std::function<UpdaterTexts()> texts;
        std::function<QWidget*()> parent;  // Elternfenster für Dialoge (darf nullptr liefern)
        std::function<bool()> beforeInstall;  // optional: false = nicht beenden (z. B. ungespeicherte Arbeit)
        std::function<void()> quit;           // optional: sauber beenden (sonst QCoreApplication::quit)
        QString innoAppId;                    // Windows: AppId des Installers (ohne {}), erkennt "alle Benutzer" / "nur ich"
    };

    explicit Updater(Options o, QObject* parent = nullptr);
    // manual = vom Nutzer angestoßen: zeigt auch ignorierte Versionen und "schon aktuell"
    void check(bool manual = false);
    // Automatisch: kurz nach dem Start und dann stündlich (scheitert eine Suche, nach 10 Minuten nochmal)
    void startAutoCheck(int firstDelayMs);

private:
    void onRelease(const QJsonObject& rel, bool manual);
    QString pickAsset(const QJsonObject& rel, QString* url) const;
    void download(const QString& url, const QString& name, const QString& page, const QString& tag);
    bool launchInstaller(const QString& file);
    void fail(const QString& page);

    Options o_;
    QNetworkAccessManager* net_;
    bool busy_ = false;        // Suche, Frage oder Download läuft: keine zweite Suche starten
    bool installing_ = false;
};
