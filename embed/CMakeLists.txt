cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(puae_m68k_embed LANGUAGES C)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(PUAE_M68K_EMBED_STANDALONE ON)
else()
    set(PUAE_M68K_EMBED_STANDALONE OFF)
endif()

option(PUAE_M68K_EMBED_BUILD_TESTS "Build standalone embed API tests"
       ${PUAE_M68K_EMBED_STANDALONE})

add_library(uae_m68k_embed STATIC
    ../sources/src/cpuemu_11.c
    ../sources/src/cpustbl.c
    ../sources/src/cpudefs.c
    ../sources/src/readcpu.c
    ../sources/src/newcpu_common.c
    ../sources/src/m68k_embed.c)
add_library(PUAE::M68kEmbed ALIAS uae_m68k_embed)

add_library(uae_m68k_embed_sections INTERFACE)
if(MSVC)
    target_compile_options(uae_m68k_embed_sections INTERFACE /Gy /Gw)
else()
    target_compile_options(uae_m68k_embed_sections INTERFACE
        -ffunction-sections -fdata-sections)
endif()
target_link_options(uae_m68k_embed_sections INTERFACE
    $<$<OR:$<PLATFORM_ID:Linux>,$<PLATFORM_ID:Android>>:-Wl,--gc-sections>
    $<$<PLATFORM_ID:Darwin>:-Wl,-dead_strip>
    $<$<PLATFORM_ID:Windows>:/OPT:REF>
    $<$<PLATFORM_ID:Windows>:/INCREMENTAL:NO>)
target_link_libraries(uae_m68k_embed PRIVATE uae_m68k_embed_sections)

target_compile_features(uae_m68k_embed PRIVATE c_std_11)
target_compile_definitions(uae_m68k_embed PRIVATE
    __LIBRETRO__
    UAE_M68K_EMBED)
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
    target_compile_options(uae_m68k_embed PRIVATE -Wno-comment)
endif()
target_include_directories(uae_m68k_embed BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(uae_m68k_embed SYSTEM
    PUBLIC ../sources/src/include
    PRIVATE
        ../sources/src
        ../retrodep
        ..
        ../deps/7zip
        ../deps/libmpeg2/include
        ../libretro
        ../libretro-common/include
        ../libretro-common/include/compat/zlib)

set_target_properties(uae_m68k_embed PROPERTIES
    C_EXTENSIONS ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON)

if(PUAE_M68K_EMBED_BUILD_TESTS)
    enable_testing()
    add_executable(uae_m68k_embed_test tests/m68k_embed_test.c)
    target_compile_definitions(uae_m68k_embed_test PRIVATE UAE UAE_M68K_EMBED __LIBRETRO__)
    target_link_libraries(uae_m68k_embed_test PRIVATE PUAE::M68kEmbed)
    add_test(NAME uae_m68k_embed_test COMMAND uae_m68k_embed_test)
    add_executable(uae_m68k_link_sections_test tests/link_sections_test.c)
    target_link_libraries(uae_m68k_link_sections_test PRIVATE uae_m68k_embed_sections)
    add_test(NAME uae_m68k_link_sections_test COMMAND uae_m68k_link_sections_test)
    foreach(profile IN ITEMS native windows)
        add_executable(uae_m68k_sysconfig_${profile}_test tests/sysconfig_test.c)
        target_compile_definitions(uae_m68k_sysconfig_${profile}_test PRIVATE UAE_M68K_EMBED)
        if(profile STREQUAL "windows")
            target_compile_definitions(uae_m68k_sysconfig_${profile}_test PRIVATE _WIN32)
        endif()
        add_test(NAME uae_m68k_sysconfig_${profile}_test
                 COMMAND uae_m68k_sysconfig_${profile}_test)
    endforeach()
endif()
