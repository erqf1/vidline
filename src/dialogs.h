#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include "model.h"
#include "presets.h"
#include "tags.h"

// Erststart: Sprache wählen
class LanguageDialog : public QDialog {
    Q_OBJECT
public:
    explicit LanguageDialog(QWidget* parent = nullptr);
    QString code() const;

private:
    QListWidget* list_;
};

// Tags einer Tondatei: Cover, Titel, Interpret ... Bilder lassen sich auch hineinziehen.
class TagDialog : public QDialog {
    Q_OBJECT
public:
    TagDialog(QWidget* parent, const QString& path, const AudioTags& tags);
    AudioTags tags() const;

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void setCover(const QImage& img);
    AudioTags tags_;
    bool coverOk_;
    QLabel* cover_;
    QPushButton* remove_;
    QLineEdit* title_;
    QLineEdit* artist_;
    QLineEdit* album_;
    QLineEdit* albumArtist_;
    QLineEdit* year_;
    QLineEdit* track_;
    QLineEdit* genre_;
};

// Einstellungen: Sprache + Design (wirkt sofort)
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void languageChanged(const QString& code);
    void themeChanged(const QString& id);
    void accentChanged(const QString& color);  // leer = Farbe des Designs
    void checkUpdates();
    void openPlugins();

private:
    void retranslate();
    void markAccent();  // gewählte Farbe umranden
    QLabel *lblLang_, *lblTheme_, *lblAccent_, *lblIcons_ = nullptr;
    QComboBox *lang_, *theme_, *icons_ = nullptr;
    QPushButton *ok_, *update_, *keys_, *plugins_, *defaults_, *accentDefault_, *accentCustom_;
    QList<QPushButton*> swatches_;
};

// Knopf, der beim Anklicken die nächste Tastenkombination aufnimmt (Esc bricht ab, Rücktaste entfernt die Taste)
class KeyButton : public QPushButton {
    Q_OBJECT
public:
    explicit KeyButton(const QKeySequence& seq, QWidget* parent = nullptr);
    QKeySequence sequence() const { return seq_; }
    void setSequence(const QKeySequence& seq);

signals:
    void sequenceChanged(const QKeySequence& seq);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void setRecording(bool on);
    QKeySequence seq_;
    bool recording_ = false;
};

// Alle Tastenkürzel ansehen und ändern - wirkt nach "OK" sofort
class ShortcutDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutDialog(QWidget* parent = nullptr);
};

// Vorschau der Zielgröße: Rahmen im Seitenverhältnis der Ausgabe, darin das Projekt (Rand, Zuschnitt, unscharf)
class FramePreview : public QWidget {
public:
    FramePreview() { setFixedSize(132, 132); }
    void set(QSize canvas, QSize target, int fit) { canvas_ = canvas; target_ = target; fit_ = fit; update(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QSize canvas_{16, 9}, target_{16, 9};
    int fit_ = 0;
};

// Export-Optionen: Vorlage oder Auflösung / Bildrate / Qualität – nur Werte bis zum Original werden angeboten
class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(QWidget* parent, int srcW, int srcH, double srcFps, const QString& sourcePath);
    ExportOptions options() const;
    QString outputPath() const;      // Ziel (bei "Original ersetzen": der spätere Dateiname)
    bool replaceOriginal() const;
    void accept() override;
    // Für die ungefähre Dateigröße: Länge des Ergebnisses, Original (Länge, Größe in Pixeln), Ton ja/nein
    void setEstimateInputs(double outDuration, double srcDuration, int natW, int natH, bool hasAudio);
    void setHasSubtitles(bool on);
    bool twoPass() const;
    double maxBytes() const;
    int subMode() const;             // 0 keine, 1 einbrennen, 2 als Untertitelspur
    bool subSidecar() const;         // zusätzlich .srt neben die Datei
    QString presetId() const;
    void selectPreset(const QString& id);  // auch für Tests
    QString estimateText() const;
    void setFit(int fit);

private:
    void updateEstimate();
    void fillPresets(const QString& select);
    void applyPreset();
    void savePreset();
    const ExportPreset* currentPreset() const;
    int srcW_, srcH_;
    double srcFps_ = 30, outDur_ = 0, srcDur_ = 0, srcVideoBps_ = 0, srcPxRate_ = 0;
    class QProcess* probe_ = nullptr;  // Probe-Export (3 s) für eine echte Größenschätzung
    void showSize(double bits, const QString& extra = QString());
    bool hasAudio_ = true;
    QLabel* est_ = nullptr;
    QLabel* target_ = nullptr;
    FramePreview* preview_ = nullptr;
    QString source_;
    QString ext() const;
    QComboBox *preset_, *fit_, *format_, *res_, *fps_, *quality_, *subs_;
    QPushButton *savePreset_, *delPreset_;
    QCheckBox* sidecar_;
    QList<ExportPreset> presets_;
    class QFormLayout* form_ = nullptr;
    QLabel* ext_;
    QLineEdit *name_, *folder_;
    QPushButton* browse_;
    QCheckBox* replace_;
};
