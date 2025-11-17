cmake_minimum_required(VERSION 3.16)
project(NovaWM C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb xcb-keysyms)

add_executable(novawm
    src/main.c
    src/x11.c
    src/input.c
    src/manage.c
    src/layout.c
    src/config.c
    src/util.c
)

target_include_directories(novawm PRIVATE
    include
    ${XCB_INCLUDE_DIRS}
)

target_compile_options(novawm PRIVATE
    ${XCB_CFLAGS_OTHER}
)

target_link_libraries(novawm PRIVATE
    ${XCB_LIBRARIES}
)