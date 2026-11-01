#include "tags.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include "model.h"

bool canEmbedCover(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "mp3" || ext == "m4a" || ext == "flac";
}

AudioTags readAudioTags(const QString& path) {
    AudioTags t;
    QTemporaryDir tmp;
    const QString jpg = tmp.filePath("cover.jpg");
    QProcess p;
    // "0:v:0?": nur wenn ein Bild eingebettet ist; ohne Bild bricht ffmpeg ab, die Tags stehen trotzdem in der Ausgabe
    p.start(ffmpegPath(), {"-hide_banner", "-y", "-i", path, "-map", "0:v:0?", "-frames:v", "1", "-an", jpg});
    p.waitForFinished(20000);
    // Tags stehen vor "Duration:", bei ogg und opus an der Tonspur. Die Angaben der Bildspur gehören zum Cover.
    bool use = true;
    for (const QString& line : QString::fromUtf8(p.readAllStandardError()).split('\n')) {
        const QString s = line.trimmed();
        if (s.startsWith("Output #")) break;
        if (s.startsWith("Stream #")) use = s.contains(": Audio:");
        if (!use) continue;
        const int colon = s.indexOf(':');
        if (colon < 0) continue;
        const QString key = s.left(colon).trimmed().toLower(), val = s.mid(colon + 1).trimmed();
        if (val.isEmpty()) continue;
        auto take = [&](QString& field) { if (field.isEmpty()) field = val; };
        if (key == "title") take(t.title);
        else if (key == "artist") take(t.artist);
        else if (key == "album") take(t.album);
        else if (key == "album_artist") take(t.albumArtist);
        else if (key == "date" || key == "year") take(t.year);
        else if (key == "track") take(t.track);
        else if (key == "genre") take(t.genre);
    }
    t.cover = QImage(jpg);
    return t;
}

bool writeAudioTags(const QString& path, const AudioTags& t, QString* error) {
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();
    const QString out = fi.dir().filePath(fi.completeBaseName() + ".vidline-tmp." + fi.suffix());
    const bool embed = canEmbedCover(path), newCover = embed && t.coverChanged && !t.cover.isNull();
    QTemporaryDir tmp;
    QStringList a = {"-hide_banner", "-v", "error", "-y", "-i", path};
    if (newCover) {
        // Als JPEG: geht in allen drei Formaten. Durchsichtiges wird schwarz, größer als 1400 px bringt im Fenster nichts.
        QImage img = t.cover.width() > 1400 || t.cover.height() > 1400
                         ? t.cover.scaled(1400, 1400, Qt::KeepAspectRatio, Qt::SmoothTransformation) : t.cover;
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::black);
        QPainter g(&flat);
        g.drawImage(0, 0, img);
        g.end();
        const QString jpg = tmp.filePath("cover.jpg");
        if (!flat.save(jpg, "JPG", 92)) {
            if (error) *error = jpg;
            return false;
        }
        a << "-i" << jpg;
    }
    a << "-map" << "0:a";
    if (newCover) a << "-map" << "1:v:0";
    else if (!(embed && t.coverChanged)) a << "-map" << "0:v?";  // Bild bleibt, wie es ist
    a << "-c" << "copy" << "-map_metadata" << "0";
    const QList<std::pair<const char*, QString>> fields = {
        {"title", t.title}, {"artist", t.artist}, {"album", t.album}, {"album_artist", t.albumArtist},
        {"date", t.year}, {"track", t.track}, {"genre", t.genre}};
    // ogg und opus nehmen die Angaben der Tonspur und nicht die der Datei
    const bool streamTags = ext == "ogg" || ext == "oga" || ext == "opus";
    for (const auto& [key, val] : fields) {
        a << "-metadata" << QString("%1=%2").arg(key, val.trimmed());
        if (streamTags) a << "-metadata:s:a:0" << QString("%1=%2").arg(key, val.trimmed());
    }
    if (ext == "mp3") a << "-id3v2_version" << "3";
    if (newCover) a << "-disposition:v:0" << "attached_pic" << "-metadata:s:v:0" << "title=Album cover"
                    << "-metadata:s:v:0" << "comment=Cover (front)";
    a << out;

    QProcess p;
    p.start(ffmpegPath(), a);
    const bool done = p.waitForFinished(120000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    if (!done || QFileInfo(out).size() == 0) {
        if (error) *error = QString::fromUtf8(p.readAllStandardError()).trimmed().right(600);
        QFile::remove(out);
        return false;
    }
    // Windows gibt eine eben geschlossene Datei manchmal erst kurz darauf frei
    const QString keep = path + ".vidline-alt";
    bool moved = false;
    for (int i = 0; i < 30 && !moved; ++i) {
        moved = QFile::rename(path, keep);
        if (!moved) QThread::msleep(100);
    }
    if (!moved || !QFile::rename(out, path)) {
        if (moved) QFile::rename(keep, path);
        QFile::remove(out);
        if (error) *error = QDir::toNativeSeparators(path);
        return false;
    }
    QFile::remove(keep);
    return true;
}
