cmake_minimum_required(VERSION 3.6)
project(PNJ_MAIN C)

get_filename_component(PNJ_NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

add_library(main SHARED
    ${PNJ_NATIVE}/core/pnj_core.c
    ${PNJ_NATIVE}/platform/sdl/main.c
    ${PNJ_NATIVE}/platform/sdl/aac_dec.c)

# keep double-precision results identical to the JS reference (no fused multiply-add)
target_compile_options(main PRIVATE -ffp-contract=off)
if(PNJ_BYO)
    target_compile_definitions(main PRIVATE PNJ_BYO=1)   # no game data compiled in: maps come from the imported pack
endif()
target_include_directories(main PRIVATE ${PNJ_NATIVE}/core ${PNJ_NATIVE}/third_party/stb)
target_link_libraries(main PRIVATE SDL3::SDL3 m)
if(ANDROID)
    target_link_libraries(main log)
endif()
