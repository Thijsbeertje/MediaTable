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

# Keep the original target name because the Linux Toolbox launcher expects it.
add_executable(kde-media-flyout-overlay
    src/main.cpp
)

target_link_libraries(kde-media-flyout-overlay PRIVATE
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    target_compile_definitions(kde-media-flyout-overlay PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # GSMTC is exposed through C++/WinRT in the Windows SDK.
    file(GLOB _windows_sdk_cppwinrt_dirs
        "$ENV{WindowsSdkDir}/Include/*/cppwinrt")
    if(_windows_sdk_cppwinrt_dirs)
        list(SORT _windows_sdk_cppwinrt_dirs COMPARE NATURAL ORDER DESCENDING)
        list(GET _windows_sdk_cppwinrt_dirs 0 _windows_sdk_cppwinrt_dir)
        target_include_directories(kde-media-flyout-overlay SYSTEM PRIVATE
            "${_windows_sdk_cppwinrt_dir}")
    endif()

    target_link_libraries(kde-media-flyout-overlay PRIVATE windowsapp)
    set_target_properties(kde-media-flyout-overlay PROPERTIES
        OUTPUT_NAME "MediaTable"
    )
else()
    target_link_libraries(kde-media-flyout-overlay PRIVATE
        Qt6::DBus
        LayerShellQt::Interface
    )
endif()

if(MSVC)
    target_compile_options(kde-media-flyout-overlay PRIVATE /W4 /permissive-)
else()
    target_compile_options(kde-media-flyout-overlay PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS kde-media-flyout-overlay
    RUNTIME DESTINATION bin
)
