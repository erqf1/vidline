cmake_minimum_required(VERSION 3.21)
project(Vidline VERSION 1.11.0 LANGUAGES CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network)

set(VIDLINE_SOURCES
    src/main.cpp
    src/fileicons.h
    src/fileicons.cpp
    src/model.h
    src/model.cpp
    src/host.h
    src/theme.h
    src/theme.cpp
    src/i18n.h
    src/i18n.cpp
    src/icons.h
    src/icons.cpp
    src/overlay.h
    src/overlay.cpp
    src/timeline.h
    src/timeline.cpp
    src/videoview.h
    src/videoview.cpp
    src/dialogs.h
    src/dialogs.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/updater.h
    src/updater.cpp
    src/shortcuts.h
    src/shortcuts.cpp
    src/textrender.h
    src/textrender.cpp
    assets/fonts.qrc
    assets/logo.qrc
    assets/denoise.qrc
)

if(WIN32)
    add_executable(vidline WIN32 ${VIDLINE_SOURCES} assets/app.rc)
    set_target_properties(vidline PROPERTIES OUTPUT_NAME Vidline)
    # Cutline.exe aus alten Installationen startet nur noch Vidline.exe (siehe compat.cpp)
    add_executable(cutline-compat WIN32 packaging/windows/compat.cpp assets/app.rc)
    set_target_properties(cutline-compat PROPERTIES OUTPUT_NAME Cutline MSVC_RUNTIME_LIBRARY MultiThreaded)
elseif(APPLE)
    set(VIDLINE_ICNS ${CMAKE_SOURCE_DIR}/assets/icon.icns)
    set_source_files_properties(${VIDLINE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(vidline MACOSX_BUNDLE ${VIDLINE_SOURCES} ${VIDLINE_ICNS})
    set_target_properties(vidline PROPERTIES
        OUTPUT_NAME Vidline
        MACOSX_BUNDLE_BUNDLE_NAME Vidline
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.cutline
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_STRING "Vidline video editor"
        MACOSX_BUNDLE_COPYRIGHT "Apache License 2.0"
        MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/assets/Info.plist.in)
else()
    add_executable(vidline ${VIDLINE_SOURCES})
endif()

target_link_libraries(vidline PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Network)
target_compile_definitions(vidline PRIVATE APP_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(vidline PRIVATE /utf-8 /W3 /Zc:__cplusplus)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS vidline RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # Alter Name für Updates aus Versionen vor der Umbenennung
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink vidline \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/cutline)")
    install(FILES assets/vidline.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME vidline.png)
endif()
