# Desktop build (Windows, macOS, Linux). This branch is on SDL3, including the Android project (android/).
#   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
# SDL3 comes from the submodule in third_party/SDL3 when it has been checked out (static link, one self-contained binary),
# otherwise from the system (find_package). Force the system one with -DPNJ_SYSTEM_SDL=ON.
cmake_minimum_required(VERSION 3.16)
project(pacnjump C)
if(WIN32)
  enable_language(RC)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(PNJ_SYSTEM_SDL "Use the system SDL3 even if third_party/SDL3 is present" OFF)
option(PNJ_BYO "No game data compiled in: maps are read from pnj.pack" ON)

set(PNJ_SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL3")
if(NOT PNJ_SYSTEM_SDL AND EXISTS "${PNJ_SDL_DIR}/CMakeLists.txt")
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory("${PNJ_SDL_DIR}" SDL3-build EXCLUDE_FROM_ALL)
  set(PNJ_SDL_LIBS SDL3::SDL3-static)
  message(STATUS "SDL3: bundled (third_party/SDL3), static")
else()
  find_package(SDL3 REQUIRED CONFIG)
  set(PNJ_SDL_LIBS SDL3::SDL3)
  message(STATUS "SDL3: system")
endif()

set(PNJ_SOURCES core/pnj_core.c platform/sdl/main.c platform/sdl/jingle_native.c platform/sdl/aac_dec.c)
if(WIN32)
  add_executable(pacnjump WIN32 ${PNJ_SOURCES})       # no console window; main.c re-attaches to the parent console for CLI output
else()
  add_executable(pacnjump ${PNJ_SOURCES})
endif()
target_include_directories(pacnjump PRIVATE core third_party/stb)
# Version string shown in the window title, "--version" and the PNJ_HITS overlay. CI passes -DPNJ_VERSION=...; otherwise git describe.
if(NOT PNJ_VERSION)
  execute_process(COMMAND git describe --tags --always --dirty WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE PNJ_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT PNJ_VERSION)
  set(PNJ_VERSION dev)
endif()
target_compile_definitions(pacnjump PRIVATE PNJ_VERSION="${PNJ_VERSION}")
message(STATUS "version ${PNJ_VERSION}")
if(WIN32)
  # Version resource (product name and version show in the file properties, and the code signing service checks them)
  set(PNJ_RC_VER "0,0,0,0")
  if(PNJ_VERSION MATCHES "^v?([0-9]+)\\.([0-9]+)(\\.([0-9]+))?")
    set(_p3 "${CMAKE_MATCH_4}")
    if(NOT _p3)
      set(_p3 0)
    endif()
    set(PNJ_RC_VER "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${_p3},0")
  elseif(PNJ_VERSION MATCHES "^b([0-9]+)")
    set(PNJ_RC_VER "0,0,0,${CMAKE_MATCH_1}")
  endif()
  configure_file(packaging/pnj.rc.in ${CMAKE_BINARY_DIR}/pnj.rc @ONLY)
  target_sources(pacnjump PRIVATE ${CMAKE_BINARY_DIR}/pnj.rc)
endif()
if(PNJ_BYO)
  target_compile_definitions(pacnjump PRIVATE PNJ_BYO=1)
endif()
if(MSVC)
  target_compile_options(pacnjump PRIVATE /fp:precise /wd4996)
else()
  target_compile_options(pacnjump PRIVATE -ffp-contract=off)   # same double results as the JS reference
  target_link_libraries(pacnjump PRIVATE m)
  if(WIN32)
    target_link_options(pacnjump PRIVATE -static)               # MinGW: no libgcc/winpthread DLLs next to the exe
  endif()
endif()
target_link_libraries(pacnjump PRIVATE ${PNJ_SDL_LIBS})
if(VITA)
  # PlayStation Vita (VitaSDK): cmake -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake. The result is a .vpk for VitaShell or Vita3K.
  # SDL3 brings its own GXM renderer and the system stubs it needs through SDL3::SDL3-static.
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(PNJ_VITA_TITLEID "PNJN00001")
  vita_create_self(pacnjump.self pacnjump UNSAFE)
  vita_create_vpk(pacnjump.vpk ${PNJ_VITA_TITLEID} pacnjump.self
    VERSION 00.10
    NAME "Pac'N-Jump"
    FILE ${CMAKE_SOURCE_DIR}/packaging/vita/sce_sys/icon0.png sce_sys/icon0.png
    FILE ${CMAKE_SOURCE_DIR}/packaging/vita/sce_sys/livearea/contents/bg0.png sce_sys/livearea/contents/bg0.png
    FILE ${CMAKE_SOURCE_DIR}/packaging/vita/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE ${CMAKE_SOURCE_DIR}/packaging/vita/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml)
endif()
if(NINTENDO_3DS OR CMAKE_SYSTEM_NAME STREQUAL "Nintendo3DS")
  # Nintendo 3DS (devkitARM + libctru + citro2d): cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake. The result is pacnjump.3dsx for the Homebrew Launcher
  # or Azahar/Citra. SDL3 supplies audio, files, timers and threads here; drawing and input are in platform/n3ds/n3ds_gfx.h.
  target_compile_definitions(pacnjump PRIVATE PNJ_N3DS=1)
  set(PNJ_3DS_TEX_FLIP 0 CACHE STRING "vertical flip in the texture upload copy (0 or 1)")
  target_compile_definitions(pacnjump PRIVATE PNJ_N3DS_TEX_FLIP=${PNJ_3DS_TEX_FLIP})
  # OpenDSP (BSD-2-Clause, github.com/Cellenseres/OpenDSP) stands in for the dumped dspfirm.cdc, so sound works without dumping the firmware.
  # CI builds and installs it into portlibs before this configure runs.
  target_include_directories(pacnjump PRIVATE $ENV{DEVKITPRO}/libctru/include $ENV{DEVKITPRO}/portlibs/3ds/include)
  target_link_directories(pacnjump PRIVATE $ENV{DEVKITPRO}/libctru/lib $ENV{DEVKITPRO}/portlibs/3ds/lib)
  target_link_libraries(pacnjump PRIVATE opendsp citro2d citro3d ctru m)
  ctr_generate_smdh(pacnjump.smdh NAME "Pac'N-Jump" DESCRIPTION "Native port (bring your own APK)" AUTHOR "Jon Anderson"
    ICON ${CMAKE_SOURCE_DIR}/packaging/n3ds/icon.png)
  ctr_create_3dsx(pacnjump SMDH pacnjump.smdh)
endif()
if(WIN32)
  target_link_libraries(pacnjump PRIVATE mfuuid ole32)         # GUID definitions only; Media Foundation itself is loaded at run time (jingle_native.c)
endif()
if(APPLE)
  target_link_libraries(pacnjump PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
endif()
set_target_properties(pacnjump PROPERTIES OUTPUT_NAME "pacnjump")

if(APPLE)
  # `cmake --build build --target bundle` makes build/PacNJump.app (unsigned)
  set(PNJ_APP "${CMAKE_BINARY_DIR}/PacNJump.app")
  add_custom_target(bundle ALL DEPENDS pacnjump
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PNJ_APP}/Contents/MacOS" "${PNJ_APP}/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:pacnjump>" "${PNJ_APP}/Contents/MacOS/pacnjump"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/platform/sdl/Info.plist" "${PNJ_APP}/Contents/Info.plist")
endif()
