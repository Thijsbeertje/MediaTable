cmake_minimum_required(VERSION 3.20)
project(mediatable VERSION 0.5.37 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(WIN32)
    # The Windows build uses the shared Qt UI and a native top-level window.
    # It deliberately does not depend on Linux-only D-Bus or LayerShellQt.
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)
else()
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets DBus Network)
    find_package(LayerShellQt REQUIRED CONFIG)
endif()

add_executable(mediatable
    src/main.cpp
)

target_link_libraries(mediatable PRIVATE
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    target_compile_definitions(mediatable PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_link_libraries(mediatable PRIVATE
        Qt6::DBus
        LayerShellQt::Interface
    )
endif()

if(MSVC)
    target_compile_options(mediatable PRIVATE /W4 /permissive-)
else()
    target_compile_options(mediatable PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(mediatable PROPERTIES
    OUTPUT_NAME "MediaTable"
)

install(TARGETS mediatable
    RUNTIME DESTINATION bin
)
