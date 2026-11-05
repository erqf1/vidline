#pragma once

#include <QDialog>
#include <QList>
#include <QStringList>

class QCheckBox;
class QLabel;
class QPushButton;
class QTimer;

// "Als Standard-App festlegen": zeigt je Dateigruppe, welches Programm sie gerade öffnet; vorausgewählt ist nur,
// was kein anderes W-Software-Programm schon hat. Linux (xdg-mime) und macOS (LaunchServices) setzen sofort;
// Windows lässt Programme das nicht selbst tun - dort öffnet sich die Vidline-Seite in den Standard-Apps und das
// Fenster hakt live ab, was erledigt ist. Gleiches Verhalten wie in Textline, Picline und Webline.
class DefaultAppsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DefaultAppsDialog(QWidget* parent = nullptr);

    struct Group {
        QString id, labelKey;
        QStringList exts, mimes, utis;
    };
    struct State {
        bool mine = false;
        int done = 0, total = 1;
        QString current, owner;
    };

private:
    void refresh();
    void render();
    void apply();
    QList<State> readState(QString* registered);

    QList<Group> groups_;
    QList<State> state_;
    QList<QCheckBox*> boxes_;
    QList<QLabel*> nows_;
    QList<int> checked_;  // -1 = noch nicht entschieden, 0/1
    QString registered_;
    QLabel* note_ = nullptr;
    QPushButton* apply_ = nullptr;
    QPushButton* close_ = nullptr;
    QTimer* poll_ = nullptr;
};
