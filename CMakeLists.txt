cmake_minimum_required(VERSION 3.22)
project(orchestrator)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")

find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)

set(SOURCES
    src/main.cpp
    src/Logger.cpp
    src/AppConfig.cpp
    src/AppManager.cpp
    src/MediaConfig.cpp
    src/MediaManager.cpp
    src/RundownConfig.cpp
    src/StageWindow.cpp
    src/MainWindow.cpp
    src/ui/CyberTheme.cpp
    src/ui/CyberBackgroundWidget.cpp
    src/ui/CyberPanel.cpp
    src/ui/ModeSelectorScreen.cpp
    src/ui/ConfigureModeScreen.cpp
    src/ui/RehearsalModeScreen.cpp
    src/ui/ShowModeScreen.cpp
)

set(HEADERS
    src/Logger.h
    src/AppConfig.h
    src/AppManager.h
    src/MediaConfig.h
    src/MediaManager.h
    src/RundownConfig.h
    src/StageWindow.h
    src/MainWindow.h
    src/ui/CyberTheme.h
    src/ui/CyberBackgroundWidget.h
    src/ui/CyberPanel.h
    src/ui/ModeSelectorScreen.h
    src/ui/ConfigureModeScreen.h
    src/ui/RehearsalModeScreen.h
    src/ui/ShowModeScreen.h
)

add_executable(orchestrator
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

target_include_directories(orchestrator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

target_link_libraries(orchestrator
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Multimedia
    Qt::MultimediaWidgets
)

# Deploy Qt DLLs next to the executable after build
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()

    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()

    # Platform plugin
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()

    # Core Qt DLLs (Network and OpenGL needed by Multimedia/MultimediaWidgets)
    foreach (QT_LIB Core Gui Widgets Network OpenGL OpenGLWidgets Multimedia MultimediaWidgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach ()

    # Multimedia backend plugin (FFmpeg-based in Qt 6.7)
    if (EXISTS "${QT_INSTALL_PATH}/plugins/multimedia")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/multimedia/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${QT_INSTALL_PATH}/plugins/multimedia"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/multimedia/")
    endif ()

    # FFmpeg DLLs required by the multimedia backend
    file(GLOB FFMPEG_DLLS
        "${QT_INSTALL_PATH}/bin/avcodec-*.dll"
        "${QT_INSTALL_PATH}/bin/avformat-*.dll"
        "${QT_INSTALL_PATH}/bin/avutil-*.dll"
        "${QT_INSTALL_PATH}/bin/swresample-*.dll"
        "${QT_INSTALL_PATH}/bin/swscale-*.dll"
    )
    foreach (FFMPEG_DLL ${FFMPEG_DLLS})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFMPEG_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach ()
endif ()
