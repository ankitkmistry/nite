# CMakeList.txt : CMake project for nite, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15...3.31)

# Project description and versioning.
project (nite
    DESCRIPTION "nite: An immediate mode terminal renderer"
    VERSION 1.0
    LANGUAGES CXX)

# Set C++ standard to C++20.
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

# Target: nite
file (GLOB_RECURSE NITE_SRCS src/*.cpp)
file (GLOB_RECURSE NITE_INCLUDES src/*.hpp)

add_executable (nite ${NITE_SRCS} ${NITE_INCLUDES})
target_include_directories (nite PRIVATE src)

if (MSVC)
# warning level 4
# target_compile_options (nite PRIVATE /W4)
else ()
    # additional warnings
    target_compile_options (nite PRIVATE
        -Wall
        -Wextra
        -Wunused
        -Wunused-parameter
        -march=native
    )
endif ()

# Link with ncurses when NITE_USE_NCURSES is enabled
if (NITE_USE_NCURSES)
    find_package (Curses REQUIRED)
    target_link_libraries (nite PRIVATE ${CURSES_LIBRARIES})

    if (DEFINED CURSES_INCLUDE_DIRS)
        target_include_directories (nite PRIVATE ${CURSES_INCLUDE_DIRS})
    endif ()

    target_compile_definitions(nite PRIVATE NITE_USE_NCURSES)
endif ()

# Add tests and install targets if needed.
