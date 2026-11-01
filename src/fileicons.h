#pragma once
#include <QString>

// Windows: Dateisymbole (hell oder dunkel) für die Dateitypen von Vidline umschalten.
// Der Installer legt je Typ eine ProgID "Cutline.<endung>" an (Symbol: hell). Hier wird nur deren DefaultIcon in HKCU
// umgebogen - HKCU geht vor HKLM und braucht keine Adminrechte. Portable Version und andere Systeme: nichts zu tun.
bool fileIconsAvailable();
QString fileIconsMode();                    // "auto" (wie Windows), "light" oder "dark"
void setFileIconsMode(const QString& mode);  // speichern und sofort anwenden
void syncFileIcons();                       // beim Start: nur schreiben, wenn nötig
