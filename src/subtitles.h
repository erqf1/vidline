#pragma once
#include <QDialog>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSize>
#include <QString>
#include <functional>
#include <memory>
#include "model.h"

// Untertitel-Plugin: Untertitel als eigene Spur (Text + Zeiten), automatisch erkannt mit whisper.cpp (MIT, läuft
// offline auf dem eigenen Rechner). Programm und Sprachmodell werden erst beim ersten Benutzen geladen.

// ---- Untertitel-Dateien
QString subTime(double t, bool vtt);           // 00:01:02,345 (SRT) bzw. 00:01:02.345 (VTT)
double parseSubTime(const QString& s);         // "00:01:02,345", "01:02.345" … -> Sekunden, -1 bei Unsinn
QString toSrt(const QList<SubCue>& cues);
QString toVtt(const QList<SubCue>& cues);
QList<SubCue> parseSubtitles(const QString& text);  // SRT oder VTT (erkennt es selbst)
// Sortieren, leere entfernen, auf die Länge begrenzen, Überschneidungen auflösen
void normalizeCues(QList<SubCue>& cues, double total);
int cueAt(const QList<SubCue>& cues, double t);  // Index des Untertitels zur Zeit t, -1 = keiner
// Ausgabe von whisper.cpp (-oj) lesen; lang bekommt die erkannte Sprache
QList<SubCue> parseWhisperJson(const QByteArray& json, QString* lang = nullptr);

// ---- Darstellung (Vorschau und Einbrennen nutzen dasselbe Bild)
QImage renderSubtitleFrame(const QString& text, const SubStyle& st, QSize frame);
// Bilder + ffconcat-Liste fürs Einbrennen schreiben (Bildgröße = Ausgabegröße). Rückgabe: Pfad der Liste.
QString writeSubtitleBurn(const QList<SubCue>& cues, const SubStyle& st, QSize frame, double total, const QString& dir);

// ---- whisper.cpp
struct WhisperModel {
    const char* id;    // Dateiname ggml-<id>.bin
    const char* key;   // Übersetzungsschlüssel
    int mb;            // ungefähre Größe
};
const QList<WhisperModel>& whisperModels();
QString whisperDir();                       // <Daten>/plugins/whisper
QString whisperExe();                       // whisper-cli (VIDLINE_WHISPER_BIN für Tests)
QString whisperModelPath(const QString& id);
bool whisperModelReady(const QString& id);
QString whisperProgramAsset();              // Name der Release-Datei für dieses System (leer = nicht verfügbar)
QStringList whisperProgramUrls();           // Release dieser Version, dann neueste Version
QString whisperModelUrl(const QString& id); // offizielle Modelle (Hugging Face, ggerganov/whisper.cpp)
QStringList whisperArgs(const QString& model, const QString& wav, const QString& lang, bool translate,
                        const QString& outBase, int threads);
// Sprachen für die Auswahl: Code + Name in der eigenen Sprache ("auto" zuerst)
QList<QPair<QString, QString>> whisperLanguages();

class QNetworkAccessManager;
class QProcess;

// Ganzer Ablauf: Programm/Modell laden (falls nötig), Ton des Projekts als 16-kHz-WAV, whisper-cli, Ergebnis lesen
class WhisperJob : public QObject {
    Q_OBJECT
public:
    WhisperJob(const Project& pr, QString model, QString lang, bool translate, QObject* parent = nullptr);
    ~WhisperJob() override;
    void start();
    void cancel();
    bool running() const { return running_; }
    const QList<SubCue>& cues() const { return cues_; }
    QString detectedLanguage() const { return lang_; }

signals:
    void progress(const QString& status, double fraction);  // fraction < 0: unbestimmt
    void finished(bool ok, const QString& error);

private:
    void fetch(const QStringList& urls, const QString& dest, qint64 minBytes, const QString& statusKey,
               std::function<void()> next);
    void ensureProgram();
    void ensureModel();
    void extractAudio();
    void transcribe();
    void fail(const QString& err);

    Project pr_;
    QString model_, wantLang_, lang_;
    bool translate_ = false, running_ = false, cancelled_ = false;
    QList<SubCue> cues_;
    QNetworkAccessManager* net_ = nullptr;
    QProcess* proc_ = nullptr;
    class QNetworkReply* reply_ = nullptr;
    std::unique_ptr<class QTemporaryDir> tmp_;
};

class QComboBox;
class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;

class SubtitleDialog : public QDialog {
    Q_OBJECT
public:
    SubtitleDialog(const Project& pr, QWidget* parent = nullptr);
    const QList<SubCue>& cues() const { return cues_; }
    bool replaceExisting() const;
    void startForTest(const QString& model, const QString& lang);
    QString status() const;

protected:
    void reject() override;

private:
    void start();
    void fillModels();
    const Project& pr_;
    QComboBox *model_, *lang_;
    QCheckBox *translate_, *replace_;
    QLabel* status_;
    QProgressBar* bar_;
    QPushButton *go_, *cancel_;
    WhisperJob* job_ = nullptr;
    QList<SubCue> cues_;
};
