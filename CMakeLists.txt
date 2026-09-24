cmake_minimum_required(VERSION 3.19)

# Semantic version — bump here only (MAJOR.MINOR.PATCH)
# See AGENTS.md for bump rules.
project(tomoss VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

set(USB2XXX_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/usb2xxx)

qt_add_executable(tomoss
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/core/traceframe.h
    src/core/appversion.h
    src/core/linutil.h
    src/core/devicemanager.cpp
    src/core/devicemanager.h
    src/core/lincontroller.cpp
    src/core/lincontroller.h
    src/ui/devicepanel.cpp
    src/ui/devicepanel.h
    src/ui/tracepanel.cpp
    src/ui/tracepanel.h
    src/ui/linconfigpanel.cpp
    src/ui/linconfigpanel.h
    src/ui/linsendpanel.cpp
    src/ui/linsendpanel.h
    src/ui/uifont.h
)

target_include_directories(tomoss PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${USB2XXX_ROOT}/include
)

# Expose semver to code (window title, QCoreApplication::applicationVersion)
target_compile_definitions(tomoss PRIVATE
    TOMOSS_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(tomoss
    PRIVATE
        Qt::Core
        Qt::Widgets
)

if(WIN32)
    target_link_libraries(tomoss PRIVATE
        ${USB2XXX_ROOT}/lib/windows/x86_64/USB2XXX.lib
    )
    add_custom_command(TARGET tomoss POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${USB2XXX_ROOT}/lib/windows/x86_64/USB2XXX.dll
            ${USB2XXX_ROOT}/lib/windows/x86_64/libusb-1.0.dll
            ${USB2XXX_ROOT}/lib/windows/x86_64/binlog.dll
            $<TARGET_FILE_DIR:tomoss>
        COMMENT "Copying USB2XXX runtime DLLs"
    )
endif()

install(TARGETS tomoss
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET tomoss
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
