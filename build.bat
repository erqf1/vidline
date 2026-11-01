@echo off
rem Baut Vidline (Release) und erzeugt dist\Vidline-windows-x64.zip (portabel, mit Qt + ffmpeg).
setlocal
set QTDIR=C:\Qt\6.8.3\msvc2022_64
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
set VSCM=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake
set CMAKE="%VSCM%\CMake\bin\cmake.exe"
set PATH=%VSCM%\Ninja;%PATH%
%CMAKE% -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QTDIR% || exit /b 1
%CMAKE% --build build || exit /b 1
if not exist build\ffmpeg.exe (
  for /f "delims=" %%F in ('python -c "import imageio_ffmpeg as i;print(i.get_ffmpeg_exe())"') do copy /y "%%F" build\ffmpeg.exe >nul
)
"%QTDIR%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw build\Vidline.exe >nul
rem --- Paket
if exist dist\Vidline rmdir /s /q dist\Vidline
mkdir dist\Vidline
"%QTDIR%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw --dir dist\Vidline build\Vidline.exe >nul
copy /y build\Vidline.exe dist\Vidline\ >nul
copy /y build\Cutline.exe dist\Vidline\ >nul
copy /y build\ffmpeg.exe dist\Vidline\ >nul
xcopy /e /i /y /q assets\filetypes dist\Vidline\filetypes >nul
copy /y LICENSE dist\Vidline\ >nul 2>nul
copy /y README.md dist\Vidline\ >nul 2>nul
powershell -NoProfile -Command "Compress-Archive -Path dist\Vidline -DestinationPath dist\Vidline-windows-x64.zip -Force"
echo Fertig: build\Vidline.exe und dist\Vidline-windows-x64.zip
