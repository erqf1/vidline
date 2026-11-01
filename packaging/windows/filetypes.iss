; Erzeugt von tools_make_filetype_icons.py - nicht von Hand ändern. Wird in vidline.iss eingebunden.
; Je Dateityp eine ProgID (Cutline.mp3 ...) mit eigenem Dateisymbol. Unter "Öffnen mit" steht Vidline nur über diese
; ProgIDs - ein zusätzlicher Eintrag über Applications\Vidline.exe würde für alle Typen das Programmsymbol zeigen.
Root: HKA; Subkey: "Software\Classes\Cutline.mp4"; ValueType: string; ValueData: "MP4 (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mp4\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mp4.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mp4\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "Cutline.mp4"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "Cutline.mp4"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.mp4"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mp4.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp4"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp4\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\mp4_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.mov"; ValueType: string; ValueData: "MOV (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mov\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mov.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mov\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "Cutline.mov"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "Cutline.mov"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mov\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.mov"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mov"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mov\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mov.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mov"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mov\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\mov_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.mkv"; ValueType: string; ValueData: "MKV (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mkv\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mkv.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mkv\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "Cutline.mkv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "Cutline.mkv"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mkv\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.mkv"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mkv"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mkv\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mkv.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mkv"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mkv\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\mkv_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.m4v"; ValueType: string; ValueData: "M4V (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.m4v\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\m4v.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.m4v\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueName: "Cutline.m4v"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "Cutline.m4v"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.m4v\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.m4v"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4v"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4v\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\m4v.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4v"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4v\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\m4v_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.avi"; ValueType: string; ValueData: "AVI (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.avi\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\avi.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.avi\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "Cutline.avi"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "Cutline.avi"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.avi\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.avi"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.avi"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.avi\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\avi.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.avi"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.avi\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\avi_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.webm"; ValueType: string; ValueData: "WEBM (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.webm\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\webm.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.webm\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "Cutline.webm"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "Cutline.webm"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.webm\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.webm"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webm"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.webm\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\webm.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.webm"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.webm\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\webm_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.wmv"; ValueType: string; ValueData: "WMV (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.wmv\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\wmv.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.wmv\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueName: "Cutline.wmv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "Cutline.wmv"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.wmv\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.wmv"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wmv"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wmv\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\wmv.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wmv"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wmv\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\wmv_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.flv"; ValueType: string; ValueData: "FLV (video)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.flv\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\flv.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.flv\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueName: "Cutline.flv"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "Cutline.flv"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.flv\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Video"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.flv"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flv"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flv\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\flv.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flv"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flv\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\flv_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.mp3"; ValueType: string; ValueData: "MP3 (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mp3\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mp3.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.mp3\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "Cutline.mp3"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp3"; ValueData: "Cutline.mp3"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.mp3\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.mp3"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp3"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp3\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\mp3.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp3"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp3\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\mp3_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.wav"; ValueType: string; ValueData: "WAV (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.wav\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\wav.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.wav\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "Cutline.wav"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wav"; ValueData: "Cutline.wav"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.wav\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.wav"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wav"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wav\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\wav.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wav"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wav\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\wav_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.m4a"; ValueType: string; ValueData: "M4A (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.m4a\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\m4a.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.m4a\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueName: "Cutline.m4a"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4a"; ValueData: "Cutline.m4a"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.m4a\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.m4a"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4a"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.m4a\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\m4a.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4a"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4a\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\m4a_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.aac"; ValueType: string; ValueData: "AAC (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.aac\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\aac.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.aac\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: string; ValueName: "Cutline.aac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aac"; ValueData: "Cutline.aac"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.aac\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.aac"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.aac"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.aac\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\aac.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.aac"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.aac\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\aac_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.flac"; ValueType: string; ValueData: "FLAC (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.flac\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\flac.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.flac\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "Cutline.flac"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flac"; ValueData: "Cutline.flac"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.flac\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.flac"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flac"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.flac\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\flac.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flac"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flac\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\flac_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.ogg"; ValueType: string; ValueData: "OGG (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.ogg\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\ogg.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.ogg\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueName: "Cutline.ogg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogg"; ValueData: "Cutline.ogg"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.ogg\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.ogg"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ogg"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.ogg\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\ogg.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ogg"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ogg\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\ogg_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.opus"; ValueType: string; ValueData: "OPUS (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.opus\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\opus.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.opus\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: string; ValueName: "Cutline.opus"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".opus"; ValueData: "Cutline.opus"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.opus\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.opus"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.opus"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.opus\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\opus.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.opus"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.opus\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\opus_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.wma"; ValueType: string; ValueData: "WMA (audio)"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.wma\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\wma.ico"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.wma\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: string; ValueName: "Cutline.wma"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Vidline\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wma"; ValueData: "Cutline.wma"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.wma\OpenWithList\Cutline.exe"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: none; ValueName: "Cutline.Audio"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Classes\Cutline.wma"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wma"; Flags: uninsdeletekeyifempty; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.wma\DefaultIcon"; ValueType: string; ValueData: "{app}\filetypes\light\wma.ico"; Flags: uninsdeletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wma"; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wma\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\Classes\wma_auto_file\DefaultIcon"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Applications\Cutline.exe\SupportedTypes"; ValueType: none; Flags: deletekey
; Sammel-ProgIDs älterer Versionen bleiben (falls Windows sie als Standard gespeichert hat), ohne eigenes Symbol
Root: HKA; Subkey: "Software\Classes\Cutline.Video"; ValueType: string; ValueData: "Video"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.Video\DefaultIcon"; ValueType: none; Flags: deletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\Cutline.Video\DefaultIcon"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\Cutline.Video\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\Cutline.Video"; Flags: uninsdeletekey dontcreatekey
Root: HKA; Subkey: "Software\Classes\Cutline.Audio"; ValueType: string; ValueData: "Audio"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\Cutline.Audio\DefaultIcon"; ValueType: none; Flags: deletekey; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\Cutline.Audio\DefaultIcon"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\Cutline.Audio\shell\open\command"; ValueType: string; ValueData: """{app}\Vidline.exe"" ""%1"""; Tasks: openwith
Root: HKCU; Subkey: "Software\Classes\Cutline.Audio"; Flags: uninsdeletekey dontcreatekey
