#pragma once
#include <QDialog>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QProcess;
class QPushButton;
class QProgressBar;
class QTimer;
class QVBoxLayout;
class QWidget;

// Video-Download-Plugin: Videos (und Ton) von YouTube, Instagram, TikTok, X, Twitch, Vimeo und vielen weiteren Seiten
// per Link laden - mit Auswahl von Qualität, Format, Untertiteln und Abschnitt. Arbeitet mit yt-dlp (wie das Musik-Plugin).

// Was ein Link enthält (aus "yt-dlp -J")
struct VdlInfo {
    bool ok = false;
    bool playlist = false;
    QString title, uploader, site, thumb, error;
    double duration = 0;
    int count = 0;                 // Videos in der Playlist
    QList<int> heights;            // verfügbare Auflösungen, absteigend (z. B. 2160, 1440, 1080 …)
    QMap<int, int> fpsAt;          // höchste Bildrate je Auflösung
    QMap<int, qint64> videoSize;   // geschätzte Größe des besten Videostroms je Auflösung
    QMap<int, qint64> videoSizeH264;
    QSet<int> h264Heights;         // Auflösungen, die es in H.264 gibt
    qint64 audioSize = 0;          // bester Tonstrom
    QStringList subs;              // echte Untertitel (Sprachcodes)
    QStringList autoSubs;          // automatische Untertitel (nur gängige Sprachen)
};
VdlInfo parseVdlInfo(const QByteArray& json);

struct VdlOptions {
    int what = 0;                  // 0 Video mit Ton, 1 nur Video, 2 nur Ton
    int height = 0;                // 0 = beste
    QString container = "mp4";     // mp4, mkv, webm
    bool compat = false;           // H.264/AAC bevorzugen (läuft überall)
    QString audioFmt = "mp3";      // mp3, m4a, opus, flac, wav
    int audioKbps = 0;             // 0 = beste
    QString subLang;               // leer = keine Untertitel
    bool autoSub = false;
    QString from, to;              // Abschnitt "m:ss" (leer = alles)
    bool playlist = false;
    bool embed = true;             // Cover, Kapitel und Infos einbetten
    QString cookies;               // Browser für Anmeldung (leer = keine)
    QString dir;
};
QStringList vdlArgs(const VdlOptions& o, const QString& url, const QString& ffmpeg);
// "1:30" / "90" / "1:02:03" -> Sekunden, -1 bei Unsinn
double vdlParseTime(const QString& s);
QString vdlSize(qint64 bytes);

class VideoDownloadDialog : public QDialog {
    Q_OBJECT
public:
    explicit VideoDownloadDialog(QWidget* parent = nullptr);
    ~VideoDownloadDialog() override;

    // für Tests
    void analyze(const QString& url);
    void setOptionsForTest(const VdlOptions& o);
    void download();
    const VdlInfo& info() const { return info_; }
    bool analyzing() const;
    bool busy() const;  // läuft noch ein Download (oder wartet einer)?
    QString status() const;
    QStringList finishedFiles() const;
    QComboBox* qualityBox() const { return quality_; }
    bool hasThumb() const;

signals:
    void openFile(const QString& path);

protected:
    void closeEvent(class QCloseEvent* e) override;
    void reject() override;

private:
    struct Job {
        QString url, title;
        VdlOptions opt;
        int state = 0;  // 0 wartet, 1 lädt, 2 fertig, 3 fehlgeschlagen, 4 abgebrochen
        int part = 0, parts = 1;
        double pct = 0;
        QString line, error;
        QStringList files;
        qint64 startedAt = 0;
        QFrame* row = nullptr;
        QLabel *name = nullptr, *state_ = nullptr;
        QProgressBar* bar = nullptr;
        QPushButton *cancel = nullptr, *open = nullptr, *reveal = nullptr;
    };

    void ensureYtDlp(std::function<void(bool, QString)> done);
    void runAnalyze();
    void showInfo();
    void fillQuality();
    void fillFormats();
    void updateOptionVisibility();
    VdlOptions currentOptions() const;
    void saveOptions() const;
    void addJobRow(Job* j);
    void updateJobRow(Job* j);
    void pump();
    void startJob(Job* j);
    void setStatus(const QString& s);

    QNetworkAccessManager* net_ = nullptr;
    QLineEdit* url_;
    QPushButton *check_, *start_, *close_, *pick_;
    QLabel *status_, *thumb_, *title_, *sub_, *folder_, *note_;
    QFrame* card_;
    QWidget* opts_;
    QComboBox *what_, *quality_, *format_, *subs_, *cookies_;
    QLabel *lblQuality_, *lblFormat_, *lblSubs_;
    QCheckBox *compat_, *section_, *playlist_, *embed_;
    QLineEdit *from_, *to_;
    QWidget* sectionRow_;
    QFrame* queueBox_;
    QVBoxLayout* queue_;
    QTimer* debounce_;
    QProcess* probe_ = nullptr;
    QProcess* proc_ = nullptr;
    Job* running_ = nullptr;
    QList<Job*> jobs_;
    VdlInfo info_;
    QString infoUrl_, dir_;
    bool settingUp_ = false;
};
