# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Force Ninja to use response files to avoid Windows CreateProcess 32KB command length limits
set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Force Ninja response files on Windows" FORCE)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES ON CACHE BOOL "Use response file for C includes" FORCE)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES ON CACHE BOOL "Use response file for CXX includes" FORCE)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.5.2")
project(xiaozhi)
