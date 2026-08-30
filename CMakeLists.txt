cmake_minimum_required(VERSION 3.19)
project(ScreenRec VERSION 1.2.1 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia)

qt_standard_project_setup()

set(app_icon_windows "")
set(app_version_windows "")
if (WIN32)
    enable_language(RC)
    set(app_icon_windows "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc")
    set_source_files_properties(${app_icon_windows} PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.ico"
    )
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")

    set(SCREENREC_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(SCREENREC_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(SCREENREC_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    if (SCREENREC_VERSION_MINOR STREQUAL "")
        set(SCREENREC_VERSION_MINOR 0)
    endif()
    if (SCREENREC_VERSION_PATCH STREQUAL "")
        set(SCREENREC_VERSION_PATCH 0)
    endif()
    set(app_version_windows "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/version.rc.in"
        "${app_version_windows}"
        @ONLY
    )
endif()

qt_add_executable(ScreenRec
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/appstyle.cpp
    src/ui/appstyle.h
    src/ui/previewwidget.cpp
    src/ui/previewwidget.h
    src/ui/countdownoverlay.cpp
    src/ui/countdownoverlay.h
    src/ui/regionselector.cpp
    src/ui/regionselector.h
    src/ui/floatingbar.cpp
    src/ui/floatingbar.h
    src/ui/audiochip.cpp
    src/ui/audiochip.h
    src/capture/audiolevelmonitor.cpp
    src/capture/audiolevelmonitor.h
    src/core/recordingcontroller.cpp
    src/core/recordingcontroller.h
    src/core/settings.cpp
    src/core/settings.h
    src/core/hotkeymanager.cpp
    src/core/hotkeymanager.h
    src/core/captureexclude.cpp
    src/core/captureexclude.h
    src/capture/screencapturesource.cpp
    src/capture/screencapturesource.h
    src/capture/systemaudiocapture.cpp
    src/capture/systemaudiocapture.h
    ${app_icon_windows}
    ${app_version_windows}
)

qt_add_resources(ScreenRec "icons"
    PREFIX "/"
    FILES assets/app.png
)

target_include_directories(ScreenRec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capture
)

target_compile_definitions(ScreenRec PRIVATE
    SCREENREC_VERSION="${PROJECT_VERSION}"
)

target_compile_features(ScreenRec PRIVATE cxx_std_17)

if (MINGW)
    target_compile_options(ScreenRec PRIVATE -finput-charset=UTF-8)
endif()

target_link_libraries(ScreenRec
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
)

if (WIN32)
    target_link_libraries(ScreenRec PRIVATE ole32 uuid user32)
endif()

install(TARGETS ScreenRec
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ScreenRec
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
