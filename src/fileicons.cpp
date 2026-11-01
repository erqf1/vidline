#include "fileicons.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

static QString iconDir(const QString& variant) {
    return QCoreApplication::applicationDirPath() + "/filetypes/" + variant;
}

bool fileIconsAvailable() {
#ifdef Q_OS_WIN
    if (!QFileInfo(iconDir("light")).isDir()) return false;
    // Nur, wenn der Installer die Dateitypen eingetragen hat (nicht bei der portablen Version)
    const QSettings reg("HKEY_CLASSES_ROOT\\Cutline.mp3\\shell\\open\\command", QSettings::NativeFormat);
    return reg.contains("Default");
#else
    return false;
#endif
}

QString fileIconsMode() { return QSettings().value("fileIcons", "auto").toString(); }

static QString resolved(const QString& mode) {
    if (mode == "light" || mode == "dark") return mode;
#ifdef Q_OS_WIN
    // Windows-Modus für Apps (wie der Explorer): AppsUseLightTheme = 0 -> dunkel
    const QSettings p("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    return p.value("AppsUseLightTheme", 1).toInt() == 0 ? "dark" : "light";
#else
    return "light";
#endif
}

static bool apply(const QString& variant) {
#ifdef Q_OS_WIN
    const QFileInfoList icons = QDir(iconDir(variant)).entryInfoList({"*.ico"}, QDir::Files);
    if (icons.isEmpty()) return false;
    const QString classes = "HKEY_CURRENT_USER\\Software\\Classes\\";
    for (const QFileInfo& f : icons) {
        const QString ext = f.completeBaseName(), path = QDir::toNativeSeparators(f.absoluteFilePath());
        QSettings(classes + "Cutline." + ext + "\\DefaultIcon", QSettings::NativeFormat).setValue("Default", path);
        // Symbol je Endung: greift, wenn der gewählte Eintrag selbst keins hat - z. B. die Sammel-ProgIDs
        // Cutline.Video/Audio älterer Versionen (sonst stünde bei jedem Video "VIDEO" statt "MP4")
        QSettings(classes + "SystemFileAssociations\\." + ext + "\\DefaultIcon", QSettings::NativeFormat).setValue("Default", path);
        // Hat man Vidline.exe über "Andere App auswählen" gewählt, legt Windows "<endung>_auto_file" ohne Symbol an
        // und der Explorer zeigt das Programmsymbol - diesem Eintrag das Dateisymbol geben
        const QString autoFile = classes + ext + "_auto_file";
        const QString cmd = QSettings(autoFile + "\\shell\\open\\command", QSettings::NativeFormat).value("Default").toString();
        if (cmd.contains("Vidline.exe", Qt::CaseInsensitive) || cmd.contains("Cutline.exe", Qt::CaseInsensitive))
            QSettings(autoFile + "\\DefaultIcon", QSettings::NativeFormat).setValue("Default", path);
    }
    for (const char* kind : {"Video", "Audio"})  // Sammel-ProgIDs älterer Versionen: kein eigenes Symbol (siehe oben)
        QSettings(classes + "Cutline." + kind, QSettings::NativeFormat).remove("DefaultIcon");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);  // Explorer zeichnet die Symbole neu
    QSettings().setValue("fileIconsApplied", variant);
    QSettings().setValue("fileIconsVersion", APP_VERSION);
    return true;
#else
    Q_UNUSED(variant);
    return false;
#endif
}

void setFileIconsMode(const QString& mode) {
    QSettings().setValue("fileIcons", mode);
    if (fileIconsAvailable()) apply(resolved(mode));
}

void syncFileIcons() {
    if (!fileIconsAvailable()) return;
    const QString want = resolved(fileIconsMode());
    // Bei Wechsel hell/dunkel und einmal nach jedem Update (der Installer schreibt wieder hell und kennt die
    // "_auto_file"-Einträge des Benutzers nicht)
    const QSettings s;
    if (s.value("fileIconsApplied").toString() == want && s.value("fileIconsVersion").toString() == APP_VERSION) return;
    apply(want);
}
