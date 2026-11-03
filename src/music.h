#pragma once
#include <QDialog>
#include <QFrame>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QNetworkAccessManager;
class QPlainTextEdit;
class QProcess;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTimer;

// ---------------------------------------------------------------- Musik-Plugin
// Bibliothek mit Songs und Playlists. Die Dateien bleiben, wo sie sind; gespeichert werden nur Angaben und Cover
// (verkleinert) im Datenordner von Vidline.
struct Song {
    QString path, title, artist, album, track;
    double duration = 0;
    qint64 added = 0;  // Millisekunden seit 1970
    QString cover;     // Dateiname im Cover-Ordner, leer = keins
    QString name() const;  // Titel oder Dateiname
};

struct Playlist {
    QString id, name;
    QStringList paths;
};

QString musicDataDir();      // Bibliothek, Cover und yt-dlp (VIDLINE_DATA für Tests)
QString musicDownloadDir();  // Ziel für Downloads, Standard: Musik/Vidline
QStringList audioPatterns(); // *.mp3 *.flac … für Dateiauswahl und Ordnersuche

class MusicLibrary : public QObject {
    Q_OBJECT
public:
    explicit MusicLibrary(QObject* parent = nullptr);
    ~MusicLibrary() override;

    const QList<Song>& songs() const { return songs_; }
    const Song* find(const QString& path) const;
    const QList<Playlist>& playlists() const { return lists_; }
    const Playlist* playlist(const QString& id) const;
    QString coverFile(const Song& s) const;  // voller Pfad oder leer

    // Dateien und Ordner aufnehmen; die Angaben werden im Hintergrund mit ffmpeg gelesen
    void import(const QStringList& paths, const QString& toPlaylist = QString());
    void refresh(const QString& path);  // Tags neu lesen, etwa nach dem Bearbeiten
    void remove(const QStringList& paths);
    bool busy() const { return total_ > 0; }

    QString createPlaylist(const QString& name);
    void renamePlaylist(const QString& id, const QString& name);
    void deletePlaylist(const QString& id);
    void addToPlaylist(const QString& id, const QStringList& paths);
    void removeFromPlaylist(const QString& id, int index);
    void movePlaylistItem(const QString& id, int from, int to);

signals:
    void changed();
    void progress(int done, int total);  // total 0 = fertig

private:
    struct Job { QString path, playlist; bool refresh = false; };
    void load();
    void save();
    void touch();  // speichern und Bescheid sagen, beides gesammelt
    void scanNext();
    void scanned(const Job& job, const QString& err, const QString& coverName);

    QList<Song> songs_;
    QList<Playlist> lists_;
    QList<Job> queue_;
    QList<QProcess*> running_;
    int done_ = 0, total_ = 0;
    QTimer* saveTimer_;
};

// Liste links neben dem Player: Songs, Alben, Interpreten, Playlists
class MusicPanel : public QFrame {
    Q_OBJECT
public:
    explicit MusicPanel(MusicLibrary* lib, QWidget* parent = nullptr);
    void retranslate();
    void setNowPlaying(const QString& path);
    // für Tests und Bildschirmfotos
    void showView(int view);
    void openGroup(const QString& kind, const QString& key);
    void setSearch(const QString& text);
    int visibleCount() const;

signals:
    void play(const QStringList& paths, int index);
    void playNext(const QStringList& paths);
    void enqueue(const QStringList& paths);
    void downloadRequested();

protected:
    void dragEnterEvent(class QDragEnterEvent*) override;
    void dropEvent(class QDropEvent*) override;

private:
    enum View { Songs, Albums, Artists, Lists };
    void rebuild();
    void fillSongs();
    void fillGrid();
    QList<const Song*> groupSongs(const QString& kind, const QString& key) const;
    QStringList shownPaths() const;
    QStringList selectedPaths() const;
    void songMenu(const QPoint& pos);
    void gridMenu(const QPoint& pos);
    void gridActivated(QListWidgetItem* it);
    void newPlaylist(const QStringList& paths);
    QPixmap cover(const Song* s, int size);
    QPixmap placeholder(int size, bool round);

    MusicLibrary* lib_;
    QLabel *title_, *crumb_, *count_, *empty_;
    QPushButton *import_, *link_, *back_, *playAll_;
    QPushButton* tabs_[4];
    QWidget* crumbRow_;
    QLineEdit* search_;
    QComboBox* sort_;
    QStackedWidget* stack_;
    QListWidget *songs_, *grid_;
    QProgressBar* progress_;
    QTimer* rebuildTimer_;
    int view_ = Songs;
    QString groupKind_, groupKey_;  // geöffnetes Album, Interpret oder Playlist
    QString nowPlaying_;
    QHash<QString, QPixmap> covers_;
};

// yt-dlp für Links. Wird beim ersten Benutzen von GitHub geladen und danach selbst aktuell gehalten.
QString ytDlpPath();

class DownloadDialog : public QDialog {
    Q_OBJECT
public:
    DownloadDialog(MusicLibrary* lib, QWidget* parent = nullptr);
    ~DownloadDialog() override;
    void startWith(const QString& url, bool wholeList);  // für Tests
    bool running() const;
    QString status() const;
    QStringList files() const { return files_; }

private:
    void start();
    void setup();      // yt-dlp laden
    void maybeUpdate();
    void run();
    void finish(bool ok, const QString& message);
    void setBusy(bool on);

    MusicLibrary* lib_;
    QLineEdit* url_;
    QComboBox* fmt_;
    QCheckBox* whole_;
    QLabel *folder_, *status_;
    QPushButton *pick_, *start_, *close_;
    QProgressBar* bar_;
    QPlainTextEdit* log_;
    QProcess* proc_ = nullptr;
    QNetworkAccessManager* net_ = nullptr;
    QString dir_, listTitle_, lastError_;
    QStringList files_;
    qint64 startedAt_ = 0;
};
