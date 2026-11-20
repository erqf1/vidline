// Alt-Projekte für den Regressionstest (gleich in mainwindow-Tests)
#include "../model.h"
static QList<QPair<QString, QStringList>> legacyCases() {
    QList<QPair<QString, QStringList>> out;
    auto src = [](const char* p, double d, bool a, bool v = true) { Source s; s.path = p; s.duration = d; s.fps = 30; s.hasAudio = a; s.video = v; s.probed = true; return s; };
    {
        Project pr; pr.sources << src("/in/a.mp4", 10, true);
        pr.pieces << Piece{0, 0, 10, 1, 1};
        ExportOptions o; o.width = 1280; o.height = 720;
        out << qMakePair(QString("eins"), buildExport(pr, o, "/out/x.mp4"));
    }
    {
        Project pr; pr.sources << src("/in/a.mp4", 10, true) << src("/in/b.mov", 5, false) << src("/in/c.mp3", 8, true, false);
        Piece p1{0, 1, 4, 2.0, 0.5}; Piece p2{1, 0, 5, 1, 1}; p2.scale = 0.8; p2.px = 0.1; p2.rot = 15;
        Piece p3{2, 0, 3, 0.5, 1.5}; Piece p4{0, 6, 9, 1, 1};
        pr.pieces << p1 << p2 << p3 << p4;
        Item im; im.id = 5; im.kind = Item::Image; im.path = "/in/logo.png"; im.t0 = 0.5; im.t1 = 3; im.x = 0.1; im.y = 0.1; im.w = 0.2; im.h = 0.1;
        Item bl; bl.id = 6; bl.kind = Item::Blur; bl.t0 = 1; bl.t1 = 2; bl.x = 0.5; bl.y = 0.5; bl.w = 0.3; bl.h = 0.2; bl.strength = 40;
        Item tx; tx.id = 7; tx.kind = Item::Text; tx.path = "/tmp/text7.png"; tx.t0 = 2; tx.t1 = 4; tx.x = 0.2; tx.y = 0.7; tx.w = 0.5; tx.h = 0.1;
        pr.items << im << bl << tx;
        AudioClip a; a.id = 8; a.path = "/in/m.mp3"; a.t0 = 1.5; a.srcStart = 2; a.dur = 4; a.fileDur = 30; a.volume = 0.7;
        AudioClip b; b.id = 9; b.path = "/in/v.wav"; b.t0 = 0; b.srcStart = 0; b.dur = 2; b.fileDur = 2; b.volume = 1.2;
        pr.audios << a << b;
        ExportOptions o; o.width = 1920; o.height = 1080; o.fps = 25; o.quality = 0;
        out << qMakePair(QString("viele"), buildExport(pr, o, "/out/x.mp4"));
        o.format = "gif";
        out << qMakePair(QString("gif"), buildExport(pr, o, "/out/x.gif"));
        o.format = "mp3"; o.quality = 2;
        out << qMakePair(QString("mp3"), buildExport(pr, o, "/out/x.mp3"));
        o.format = "mkv"; o.encoder = "h264_nvenc"; o.quality = 1;
        out << qMakePair(QString("mkv"), buildExport(pr, o, "/out/x.mkv"));
    }
    {
        Project pr; pr.sources << src("/in/s.mp4", 4, false);
        pr.pieces << Piece{0, 0, 4, 1, 1};
        AudioClip a; a.id = 3; a.path = "/in/m.mp3"; a.t0 = 0; a.dur = 4; a.fileDur = 4;
        pr.audios << a;
        ExportOptions o; o.width = 1080; o.height = 1920; o.format = "mov";
        out << qMakePair(QString("stumm"), buildExport(pr, o, "/out/x.mov"));
    }
    return out;
}
