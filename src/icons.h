#pragma once
#include <QColor>
#include <QIcon>

enum class Ic {
    Open, AddVideo, AddAudio, Edit, Export, Settings, Play, Pause, Fullscreen, ExitFullscreen,
    Scissors, Trash, TrimStart, TrimEnd, Blur, Image, Undo, Redo, Volume, Music, ZoomIn, ZoomOut, Text, ChevronDown, ChevronUp,
    Pen, Shapes, Library, Prev, Next, Shuffle, Repeat, RepeatOne, Plugin, Download
};

// Vektor-Icons, mit QPainter gezeichnet (kein Emoji, keine Bilddateien).
QIcon makeIcon(Ic id, const QColor& color);
QPixmap makeAppPixmap(int size);
