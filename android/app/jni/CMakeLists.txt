cmake_minimum_required(VERSION 3.6)
project(PNJ)

# native/ root = three levels up from android/app/jni
get_filename_component(PNJ_NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# SDL 3.4.20 lives in native/third_party/SDL3 (shared build, same as SDL's own android template)
add_subdirectory(${PNJ_NATIVE}/third_party/SDL3 SDL3-build)

# the game: portable core + SDL shell, shared with the desktop build
add_subdirectory(src)
