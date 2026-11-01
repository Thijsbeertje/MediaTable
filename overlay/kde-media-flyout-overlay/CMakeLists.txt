cmake_minimum_required(VERSION 3.20)
project(mediatable VERSION 0.5.37 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets DBus Network)
find_package(LayerShellQt REQUIRED CONFIG)

add_executable(kde-media-flyout-overlay
    src/main.cpp
)

target_link_libraries(kde-media-flyout-overlay PRIVATE
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network
    LayerShellQt::Interface
)

target_compile_options(kde-media-flyout-overlay PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS kde-media-flyout-overlay
    RUNTIME DESTINATION bin
)
