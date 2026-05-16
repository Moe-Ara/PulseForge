cmake_minimum_required(VERSION 3.20)

project(BazziteSound LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

add_executable(bazzite-sound
    src/main.cpp
    src/service/AudioService.cpp
    src/gui/MainWindow.cpp
    src/gui/components/EnhancementToggle.cpp
    src/audio/PipeWireBackend.cpp
)

target_include_directories(bazzite-sound PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
)
target_link_libraries(bazzite-sound PRIVATE
      Qt6::Widgets
      ${PIPEWIRE_LIBRARIES}
)

target_compile_options(bazzite-sound PRIVATE ${PIPEWIRE_CFLAGS_OTHER})


