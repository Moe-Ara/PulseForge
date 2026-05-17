cmake_minimum_required(VERSION 3.20)

project(PulseForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

add_executable(pulseforge
    src/main.cpp
    src/dsp/AudioProcessor.cpp
    src/dsp/BiquadFilter.cpp
    src/dsp/DynamicsProcessor.cpp
    src/dsp/ParametricEQ.cpp
    src/service/AudioService.cpp
    src/system/AutoStartManager.cpp
    src/system/ProcessRunner.cpp
    src/system/RuntimeStateStore.cpp
    src/presets/PresetFactory.cpp
    src/presets/PresetStore.cpp
    src/gui/MainWindow.cpp
    src/gui/components/CardContainer.cpp
    src/gui/components/DeviceSelector.cpp
    src/gui/components/EnhancementToggle.cpp
    src/gui/components/EqualizerPanel.cpp
    src/gui/components/PresetSelector.cpp
    src/gui/components/StatusIndicator.cpp
    src/audio/PipeWireBackend.cpp
    src/audio/PipeWireRouting.cpp
)

target_include_directories(pulseforge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
)
target_link_libraries(pulseforge PRIVATE
      Qt6::Widgets
      ${PIPEWIRE_LIBRARIES}
)

target_compile_options(pulseforge PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
target_compile_definitions(pulseforge PRIVATE
    PULSEFORGE_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/pulseforge"
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/data/pulseforge.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/pulseforge.service
    @ONLY
)

install(TARGETS pulseforge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES data/pulseforge.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES data/icons/pulseforge.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES data/metainfo/io.github.Moe_Ara.PulseForge.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(FILES src/gui/styleDark.qss
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pulseforge
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pulseforge.service
    DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user
)
