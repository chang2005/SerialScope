cmake_minimum_required(VERSION 3.21)
project(SerialScope VERSION 1.0.0 LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets SerialPort Core5Compat)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Core library: no UI dependency, unit-testable
# ---------------------------------------------------------------------------
add_library(serial_core STATIC
    src/core/SerialWorker.cpp
    src/core/DataLogger.cpp
    src/core/parser/HexCodec.cpp
    src/core/parser/AsciiValueParser.cpp
    src/core/parser/DataFrameParser.cpp
    src/core/parser/HexFieldExtractor.cpp
    src/utils/CrcCalculator.cpp
    src/utils/SettingsManager.cpp
)
target_link_libraries(serial_core PUBLIC
    Qt6::Core Qt6::SerialPort Qt6::Core5Compat
)
target_include_directories(serial_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
qt_add_executable(SerialScope
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/SerialConfigPanel.cpp
    src/ui/ReceivePanel.cpp
    src/ui/SendPanel.cpp
    src/ui/WaveformPanel.cpp
    src/ui/PlotWidget.cpp
    src/ui/Theme.cpp
    src/ui/dialogs/AboutDialog.cpp
    src/ui/dialogs/CrcToolDialog.cpp
    src/ui/dialogs/HexFieldConfigDialog.cpp
    resources/app.qrc
)
target_link_libraries(SerialScope PRIVATE serial_core Qt6::Widgets)
target_compile_definitions(SerialScope PRIVATE
    SERIALSCOPE_VERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)
set_target_properties(SerialScope PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "SerialScope"
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.chang2005.serialscope
)

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/SerialScope.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/SerialScope.rc"
        @ONLY
    )
    target_sources(SerialScope PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/SerialScope.rc")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Windows deployment helper target: cmake --build build --target deploy
# ---------------------------------------------------------------------------
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT6_INSTALL_PREFIX}/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_target(deploy
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/dist"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:SerialScope>" "${CMAKE_BINARY_DIR}/dist/"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --release
                    --no-translations
                    --no-opengl-sw
                    --no-system-d3d-compiler
                    --no-system-dxc-compiler
                    --no-network
                    --no-svg
                    --skip-plugin-types "iconengines,imageformats,networkinformation,generic,tls"
                    "${CMAKE_BINARY_DIR}/dist/$<TARGET_FILE_NAME:SerialScope>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
                    "${CMAKE_BINARY_DIR}/dist/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES"
                    "${CMAKE_BINARY_DIR}/dist/LICENSES"
            DEPENDS SerialScope
            COMMENT "Deploying SerialScope with windeployqt"
        )

        set(RELEASE_BASENAME "SerialScope-${PROJECT_VERSION}-win64")
        add_custom_target(package_release
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/stage"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${CMAKE_BINARY_DIR}/release"
                    "${CMAKE_BINARY_DIR}/stage/${RELEASE_BASENAME}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_BINARY_DIR}/dist"
                    "${CMAKE_BINARY_DIR}/stage/${RELEASE_BASENAME}"
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "${CMAKE_BINARY_DIR}/release/${RELEASE_BASENAME}.zip"
            COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/stage"
                    ${CMAKE_COMMAND} -E tar cfv
                    "${CMAKE_BINARY_DIR}/release/${RELEASE_BASENAME}.zip"
                    --format=zip -- "${RELEASE_BASENAME}"
            DEPENDS deploy
            COMMENT "Packaging ${RELEASE_BASENAME}.zip"
        )
    endif()
endif()
