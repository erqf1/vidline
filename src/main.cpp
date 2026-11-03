#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QSettings>
#include <QSlider>
#include <QTimer>
#include "dialogs.h"
#include "fileicons.h"
#include "i18n.h"
#include "icons.h"
#include "mainwindow.h"
#include "textrender.h"
#include "theme.h"

// macOS liefert "Oeffnen mit"/Doppelklick als QFileOpenEvent statt als Argument
class App : public QApplication {
public:
    using QApplication::QApplication;
    MainWindow* window = nullptr;
    QString pending;

protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::FileOpen) {
            const QString f = static_cast<QFileOpenEvent*>(e)->file();
            if (window) window->openFile(f); else pending = f;
            return true;
        }
        return QApplication::event(e);
    }
};

// Mausrad verstellt in Einstellungen und im Eigenschaften-Bereich nichts mehr: Beim Durchscrollen landete der
// Zeiger sonst auf einer Auswahlliste, einem Regler oder Zahlenfeld und hat es nebenbei umgestellt. Jetzt scrollt
// das Rad dort immer die Seite. Regler außerhalb solcher Seiten (z. B. die Zeitleiste im Player) bleiben wie sie sind.
class NoWheelChange : public QObject {
protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() != QEvent::Wheel) return false;
        auto* w = qobject_cast<QWidget*>(o);
        if (!w) return false;
        QWidget* ctl = nullptr;
        for (QWidget* x = w; x && !x->isWindow(); x = x->parentWidget())
            if (qobject_cast<QComboBox*>(x) || qobject_cast<QAbstractSpinBox*>(x) || qobject_cast<QSlider*>(x)) {
                ctl = x;
                break;
            }
        if (!ctl) return false;
        for (QWidget* x = ctl->parentWidget(); x; x = x->parentWidget()) {
            if (auto* area = qobject_cast<QAbstractScrollArea*>(x)) {
                QCoreApplication::sendEvent(area->viewport(), e);
                return true;
            }
            if (x->isWindow()) return qobject_cast<QDialog*>(x) != nullptr;  // im Dialog ohne Bildlauf: einfach nichts tun
        }
        return false;
    }
};

extern QElapsedTimer g_startup;  // mainwindow.cpp, für VIDLINE_TRACE

int main(int argc, char* argv[]) {
    g_startup.start();
#ifdef Q_OS_WIN
    // Eigene App-ID: eigener Taskleisten-Button, nicht mit anderen Programmen gruppiert
    SetCurrentProcessExplicitAppUserModelID(L"WSoftware.Cutline");
#endif
#ifdef Q_OS_LINUX
    // Linux, vor allem integrierte Grafik unter Wayland (z. B. Hyprland): Hardware-Dekodierung (VAAPI) und die
    // GPU-Umrechnung der Videobilder lassen Qt Multimedia dort oft abstürzen. Software-Dekodierung reicht für
    // 1080p locker. Wer sie trotzdem will: VIDLINE_HW_DECODE=1. Eigene Werte der Variablen bleiben unangetastet.
    if (qEnvironmentVariable("VIDLINE_HW_DECODE") != "1") {
        if (!qEnvironmentVariableIsSet("QT_FFMPEG_DECODING_HW_DEVICE_TYPES"))
            qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");  // gesetzt, aber leer -> keine Hardware-Geräte
        if (!qEnvironmentVariableIsSet("QT_DISABLE_HW_TEXTURES_CONVERSION"))
            qputenv("QT_DISABLE_HW_TEXTURES_CONVERSION", "1");
    }
#endif
    // Tests: eigene Einstellungen statt der echten (VIDLINE_SETTINGS=<ordner>)
    if (qEnvironmentVariableIsSet("VIDLINE_SETTINGS")) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, qEnvironmentVariable("VIDLINE_SETTINGS"));
    }
    App app(argc, argv);
    app.installEventFilter(new NoWheelChange);
    // Mitgelieferte Text-Schriften erst laden, wenn man einen Text einfügt (schnellerer Start)
    QCoreApplication::setOrganizationName("WSoftware");
    QCoreApplication::setApplicationName("VideoEditor");
    QGuiApplication::setApplicationDisplayName("Vidline");

    QIcon appIcon;
    for (int s : {16, 24, 32, 48, 64, 128, 256}) appIcon.addPixmap(makeAppPixmap(s));
    app.setWindowIcon(appIcon);

    QSettings st;
    setCurrentTheme(st.value("theme", "mint").toString());
    setAccentColor(st.value("accent").toString());
    app.setStyleSheet(buildStyleSheet(currentTheme()));

    QString lang = st.value("language").toString();
    if (lang.isEmpty()) {  // Erststart: Sprache abfragen
        LanguageDialog dlg;
        dlg.exec();
        lang = dlg.code();
        st.setValue("language", lang);
    }
    setLanguage(lang);

    MainWindow w;
    w.show();
    QTimer::singleShot(3000, &w, [] { syncFileIcons(); });  // Dateisymbole hell/dunkel (Windows, installiert)
    QTimer::singleShot(0, &w, [] { extern void vidlineTrace(const char*); vidlineTrace("window shown"); });
    app.window = &w;
    if (!app.pending.isEmpty()) w.openFile(app.pending);
    // Argumente als Unicode lesen (Dateien aus "Oeffnen mit" koennen Sonderzeichen enthalten)
    const QStringList args = app.arguments();
    if (args.size() > 1 && !args[1].startsWith("--"))
        QTimer::singleShot(0, &w, [&w, path = args[1]] { w.openFile(path); });
    if (args.size() > 3 && args[2] == "--shots") w.selfShots(args[3]);
    if (args.size() > 3 && args[2] == "--aspectshots") w.aspectShots(args[3]);
    if (args.size() > 3 && args[2] == "--fsshots") w.fsShots(args[3]);
    if (args.size() > 3 && args[2] == "--tagshots") w.tagShots(args[3]);
    if (args.size() > 2 && args[1] == "--musicshots") w.musicShots(args[2]);
    if (args.size() > 3 && args[1] == "--dlshots") w.downloadShots(args[2], args[3]);
    return app.exec();
}
