#!/bin/sh
# Installs Vidline for the current user (~/.local). Needs Qt 6 (Widgets, Multimedia) from your distribution.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${HOME}/.local"
mkdir -p "$PREFIX/opt/vidline" "$PREFIX/bin" "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor/256x256/apps"
cp "$DIR/vidline" "$DIR/ffmpeg" "$PREFIX/opt/vidline/"
ln -sf "$PREFIX/opt/vidline/vidline" "$PREFIX/bin/vidline"
cp "$DIR/icon.png" "$PREFIX/share/icons/hicolor/256x256/apps/vidline.png"
sed "s|^Exec=.*|Exec=$PREFIX/opt/vidline/vidline %f|" "$DIR/vidline.desktop" > "$PREFIX/share/applications/vidline.desktop"
echo "Vidline installed. Run: vidline"
