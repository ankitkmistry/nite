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
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Target: nite
add_library (nite STATIC src/nite.cpp)
target_include_directories (nite PUBLIC include)
# INFO: Make sure nite::nite is always available
add_library (nite::nite ALIAS nite)

# Target: nite_test
# An executable target for test the library
add_executable (nite_test src/main.cpp src/stb_image.cpp)
target_include_directories (nite_test PUBLIC src)
target_link_libraries (nite_test nite::nite)

if (MSVC)
# warning level 4
# target_compile_options (nite_test PRIVATE /W4)
else ()
    # additional warnings
    target_compile_options (nite_test PRIVATE
        -Wall
        -Wextra
        -march=native
    )
endif ()

# Link with ncurses when NITE_USE_NCURSES is enabled
if (NITE_USE_NCURSES)
    find_package (Curses REQUIRED)
    target_link_libraries (nite_test PRIVATE ${CURSES_LIBRARIES})

    if (DEFINED CURSES_INCLUDE_DIRS)
        target_include_directories (nite_test PRIVATE ${CURSES_INCLUDE_DIRS})
    endif ()

    target_compile_definitions(nite_test PRIVATE NITE_USE_NCURSES)
endif ()

# Add tests and install targets if needed.
