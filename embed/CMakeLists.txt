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
if(MSVC)
    target_link_options(uae_m68k_embed_sections INTERFACE
        /OPT:REF
        /INCREMENTAL:NO)
elseif(WIN32)
    target_link_options(uae_m68k_embed_sections INTERFACE
        -Wl,--gc-sections)
elseif(UNIX)
    target_link_options(uae_m68k_embed_sections INTERFACE
        $<$<PLATFORM_ID:Linux>:-Wl,--gc-sections>
        $<$<PLATFORM_ID:Android>:-Wl,--gc-sections>
        $<$<PLATFORM_ID:Darwin>:-Wl,-dead_strip>)
endif()
target_link_libraries(uae_m68k_embed PRIVATE uae_m68k_embed_sections)

target_compile_features(uae_m68k_embed PRIVATE c_std_11)
target_compile_definitions(uae_m68k_embed PRIVATE
    __LIBRETRO__
    UAE_M68K_EMBED)
if(WIN32 AND NOT MSVC)
    # MinGW defines _WIN32 but not the legacy WIN32 macro used by retrodep's
    # CRT compatibility branch.  Keep the native _fseeki64/_ftelli64 names;
    # mapping them to POSIX fseeko64/ftello64 redeclares MinGW's CRT helpers.
    target_compile_definitions(uae_m68k_embed PRIVATE WIN32)
endif()
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
    # Link the same runtime archive without its section-GC usage requirements.
    # Every selected source object must have a complete set of owned symbols.
    add_executable(uae_m68k_complete_link_test tests/m68k_embed_test.c)
    target_compile_definitions(uae_m68k_complete_link_test PRIVATE
        UAE UAE_M68K_EMBED __LIBRETRO__)
    target_include_directories(uae_m68k_complete_link_test SYSTEM PRIVATE
        ../sources/src/include)
    target_link_libraries(uae_m68k_complete_link_test PRIVATE
        "$<TARGET_LINKER_FILE:uae_m68k_embed>")
    add_dependencies(uae_m68k_complete_link_test uae_m68k_embed)
    add_test(NAME uae_m68k_complete_link_test COMMAND uae_m68k_complete_link_test)
    # Undefined device references must fail independently of dead-section removal.
    # COFF resolves them before /OPT:REF; ELF may discard them, so this negative
    # intentionally has no per-function section policy on either host.
    try_compile(PUAE_UNOWNED_DEVICE_LINKED
        "${CMAKE_CURRENT_BINARY_DIR}/unowned-device-check"
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/link_sections_test.c"
        OUTPUT_VARIABLE PUAE_UNOWNED_DEVICE_LINK_OUTPUT)
    if(PUAE_UNOWNED_DEVICE_LINKED)
        message(FATAL_ERROR "Embedded link check accepted an unowned device reference")
    endif()
    if(NOT PUAE_UNOWNED_DEVICE_LINK_OUTPUT MATCHES "unavailable_device_owner")
        message(FATAL_ERROR
            "Embedded negative link check failed for an unrelated reason: "
            "${PUAE_UNOWNED_DEVICE_LINK_OUTPUT}")
    endif()
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
