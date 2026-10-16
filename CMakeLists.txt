cmake_minimum_required(VERSION 3.20)

# PaxDino: Rage Wars Recomp. The build is the BattleTanx: Global Assault
# recomp's (same runtime, renderer and frontend), adapted for this game.

# Windows: use Visual Studio's 64-bit-hosted clang-cl (Tools\Llvm\x64\bin);
# BUILDING.md has the exact path.

project(PaxDino LANGUAGES C CXX)

# The submodules (N64ModernRuntime, rt64, RecompFrontend) and SlotMap live in
# lib/. TRW_LIB_DIR can point elsewhere for a tree that keeps them outside the
# checkout; lib-patches/ is applied to whatever it points at.
set(TRW_LIB_DIR "${CMAKE_SOURCE_DIR}/lib" CACHE PATH "Folder holding the submodules")
foreach(submodule N64ModernRuntime RecompFrontend rt64)
    if (NOT EXISTS "${TRW_LIB_DIR}/${submodule}/CMakeLists.txt")
        message(FATAL_ERROR "${TRW_LIB_DIR}/${submodule} is empty. Run: git submodule update --init --recursive")
    endif()
endforeach()
message(STATUS "Using submodules from ${TRW_LIB_DIR}")
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()

# Keep the build machine's folders (e.g. the Windows user name) out of the
# binaries: source paths that end up in them, through __FILE__ in asserts and
# log messages or in debug info, are made relative to the repo root. Both
# slash styles are mapped, since Windows tools pass either.
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" TRW_SOURCE_DIR_NATIVE)
foreach (prefix IN ITEMS "${CMAKE_SOURCE_DIR}" "${TRW_SOURCE_DIR_NATIVE}")
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
        add_compile_options("/clang:-ffile-prefix-map=${prefix}=.")
    elseif (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options("-ffile-prefix-map=${prefix}=.")
    endif()
endforeach()

# Opt out of constexpr mutex constructor on windows to prevent vcredist issues
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (WIN32)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${TRW_LIB_DIR}/")
endif()

set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(
    HLSL_CPU
    __PRFCHWINTRIN_H
)

# Local fixes to the submodules (lib-patches/), applied at configure time.
include(${CMAKE_SOURCE_DIR}/cmake/ApplyLibPatches.cmake)
trw_apply_lib_patches(rt64 rt64)
trw_apply_lib_patches(N64Recomp N64ModernRuntime/N64Recomp)
trw_apply_lib_patches(N64ModernRuntime N64ModernRuntime)
trw_apply_lib_patches(RecompFrontend RecompFrontend)

add_subdirectory(${TRW_LIB_DIR}/rt64 ${CMAKE_BINARY_DIR}/rt64)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(${TRW_LIB_DIR}/N64ModernRuntime ${CMAKE_BINARY_DIR}/N64ModernRuntime)


target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# RecompiledFuncs - Library containing the primary recompiler output
add_library(RecompiledFuncs STATIC)

# clang-cl needs the /FI spelling of -include.
if(MSVC)
    set(TRW_RECOMP_HOOKS_FORCE_INCLUDE "SHELL:/FI${CMAKE_SOURCE_DIR}/include/trw_recomp_hooks.h")
else()
    set(TRW_RECOMP_HOOKS_FORCE_INCLUDE "SHELL:-include ${CMAKE_SOURCE_DIR}/include/trw_recomp_hooks.h")
endif()
target_compile_options(RecompiledFuncs PRIVATE
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
    -falign-functions=16
    # Declarations for the [[patches.hook]] text in turokrw.us.toml.
    ${TRW_RECOMP_HOOKS_FORCE_INCLUDE}
)

target_include_directories(RecompiledFuncs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${TRW_LIB_DIR}/N64ModernRuntime/ultramodern/include
    ${TRW_LIB_DIR}/N64ModernRuntime/librecomp/include
    ${TRW_LIB_DIR}/N64ModernRuntime/N64Recomp/include
)

# CONFIGURE_DEPENDS: regenerating can add a new funcs_N.c (more functions
# spill into another file), which a plain configure-time glob never sees --
# the build then links against a library missing that file's functions.
file(GLOB FUNC_C_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
file(GLOB FUNC_CXX_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.cpp)

if (NOT FUNC_C_SOURCES AND NOT FUNC_CXX_SOURCES)
    # Generated from the ROM by N64Recomp, so a fresh clone has none yet: an
    # empty placeholder lets the project configure to build N64Recomp first.
    message(STATUS "RecompiledFuncs/ is empty - building without the recompiled game code.")
    file(WRITE ${CMAKE_BINARY_DIR}/recompiled_funcs_placeholder.c "")
    set(FUNC_C_SOURCES ${CMAKE_BINARY_DIR}/recompiled_funcs_placeholder.c)
endif()

target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES})


# Main executable
add_executable(PaxDino)

# The frontend (src/main), the game-specific fixes (src/game) and the
# recompiled RSP audio microcode (rsp/, generated from the ROM).
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/main/*.cpp
    ${CMAKE_SOURCE_DIR}/src/game/*.cpp
    ${CMAKE_SOURCE_DIR}/rsp/*.cpp
)

# src/main/register_overlays.cpp includes RecompiledFuncs/recomp_overlays.inl,
# which a fresh clone doesn't have yet.
if (EXISTS ${CMAKE_SOURCE_DIR}/RecompiledFuncs/recomp_overlays.inl)
    target_compile_definitions(PaxDino PRIVATE TRW_HAS_RECOMPILED_FUNCS)
endif()

target_include_directories(PaxDino PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${TRW_LIB_DIR}/N64ModernRuntime/N64Recomp/include
    ${TRW_LIB_DIR}/N64ModernRuntime/thirdparty/sse2neon
    ${TRW_LIB_DIR}/rt64/src/contrib
    ${TRW_LIB_DIR}/rt64/src/contrib/hlslpp/include
    ${TRW_LIB_DIR}/rt64/src/contrib/dxc/inc
    ${TRW_LIB_DIR}/rt64/src
    ${TRW_LIB_DIR}/rt64/src/rhi
    ${TRW_LIB_DIR}/rt64/src/render
    ${TRW_LIB_DIR}/RecompFrontend/recompinput/include
    ${TRW_LIB_DIR}/RecompFrontend/recompui/include
    ${TRW_LIB_DIR}/RecompFrontend/recompui/src
    ${CMAKE_SOURCE_DIR}/third_party/audio_decoders
    ${TRW_LIB_DIR}/rt64/src/contrib/nativefiledialog-extended/src/include
    ${TRW_LIB_DIR}/SlotMap
    ${CMAKE_CURRENT_BINARY_DIR}
)

# The launcher's background art (tools/make_launcher_art.py draws it;
# src/main/launcher_art.cpp shows it).
foreach(_trw_art launcher_back.jpg launcher_front.png launcher_shade.png launcher_logo.png)
    string(REGEX REPLACE "[.]" "_" _trw_art_name "${_trw_art}")
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_trw_art_name}.c ${CMAKE_CURRENT_BINARY_DIR}/${_trw_art_name}.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/icons/${_trw_art} ${_trw_art_name} ${CMAKE_CURRENT_BINARY_DIR}/${_trw_art_name}.c ${CMAKE_CURRENT_BINARY_DIR}/${_trw_art_name}.h
        DEPENDS ${CMAKE_SOURCE_DIR}/icons/${_trw_art} file_to_c
    )
    target_sources(PaxDino PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${_trw_art_name}.c)
endforeach()

# Generate icon_bytes.c from the app icon PNG (the window icon, set in
# create_window in src/main/main.cpp).
if (EXISTS ${CMAKE_SOURCE_DIR}/icons/app.png)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/icons/app.png icon_bytes ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
        DEPENDS ${CMAKE_SOURCE_DIR}/icons/app.png
    )
    target_sources(PaxDino PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c)
    target_compile_definitions(PaxDino PRIVATE TRW_HAS_APP_ICON)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(PaxDino PRIVATE
        -march=nehalem
        -fno-strict-aliasing
        -fms-extensions
    )
else()
    target_compile_options(PaxDino PRIVATE
        -fno-strict-aliasing
        -fms-extensions
    )
endif()

if (MSVC)
    # Disable identical code folding, since this breaks mod function patching as multiple functions can get merged into one.
    target_link_options(PaxDino PRIVATE /OPT:NOICF)
endif()

if (WIN32)
    include(FetchContent)

    if (DEFINED ENV{SDL2_VERSION})
        set(SDL2_VERSION $ENV{SDL2_VERSION})
    else()
        set(SDL2_VERSION "2.30.3")
    endif()

    # Fetch SDL2 on windows
    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(PaxDino PRIVATE
        ${sdl2_SOURCE_DIR}/include
    )
    target_link_directories(PaxDino PRIVATE
        ${sdl2_SOURCE_DIR}/lib/x64
    )

    # Copy SDL2 and dxc DLLs to output folder as post build step
    add_custom_command(TARGET PaxDino POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${TRW_LIB_DIR}/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${TRW_LIB_DIR}/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:PaxDino>)

    set_target_properties(
        PaxDino
        PROPERTIES
            LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_RELWITHDEBINFO  "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )

    # The .exe icon: icons/app.res, a precompiled resource (icons/app.ico as
    # icon 1) handed straight to the linker, which accepts .res files. This
    # skips CMake's clang-cl + rc.exe resource rule, which fails here.
    # Regenerate with: llvm-rc /FO app.res <script containing: 1 ICON "app.ico">
    if (EXISTS ${CMAKE_SOURCE_DIR}/icons/app.res)
        target_link_libraries(PaxDino PRIVATE ${CMAKE_SOURCE_DIR}/icons/app.res)
    endif()
    target_link_libraries(PaxDino PRIVATE SDL2 Winmm.lib Urlmon.lib)
endif()

# The game loads assets/ relative to the working directory. A copy beside the
# exe lets it run from the build folder as well as from the repo root (e.g.
# when started or debugged from Visual Studio).
add_custom_command(TARGET PaxDino POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:PaxDino>/assets")

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(SDL2 REQUIRED)

    add_compile_definitions("PLUME_SDL_VULKAN_ENABLED")
    add_compile_definitions("RT64_SDL_WINDOW_VULKAN")

    target_include_directories(PaxDino PRIVATE ${SDL2_INCLUDE_DIRS})

    find_package(Freetype REQUIRED)

    include_directories(${FREETYPE_LIBRARIES})
    target_link_libraries(PaxDino PRIVATE ${FREETYPE_LIBRARIES} SDL2::SDL2 X11)

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    target_link_libraries(PaxDino PRIVATE "-latomic -static-libstdc++" ${CMAKE_DL_LIBS} Threads::Threads)
endif()

# DXC (the shader compiler), as RT64's own CMake script sets it up.
set (DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set (DXC_DXIL_OPTS "-Wno-ignored-attributes")
set (DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set (DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set (DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set (DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set (DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set (DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")

if (${WIN32})
    set (DXC "${TRW_LIB_DIR}/rt64/src/contrib/dxc/bin/x64/dxc.exe")
    add_compile_definitions(NOMINMAX)
else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
            set (DXC "LD_LIBRARY_PATH=${TRW_LIB_DIR}/rt64/src/contrib/dxc/lib/x64" "${TRW_LIB_DIR}/rt64/src/contrib/dxc/bin/x64/dxc-linux")
        else()
            set (DXC "LD_LIBRARY_PATH=${TRW_LIB_DIR}/rt64/src/contrib/dxc/lib/arm64" "${TRW_LIB_DIR}/rt64/src/contrib/dxc/bin/arm64/dxc-linux")
        endif()
endif()

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${TRW_LIB_DIR}/N64ModernRuntime)
set(RECOMP_FRONTEND_RT64_PATH ${TRW_LIB_DIR}/rt64)

add_subdirectory(${TRW_LIB_DIR}/RecompFrontend ${CMAKE_BINARY_DIR}/RecompFrontend)

# RmlUi uses uint32_t and friends without including <cstdint>, which clang with a
# current libstdc++ rejects: include it from here (/FI for clang-cl).
if(MSVC)
    set(TRW_CSTDINT_FORCE_INCLUDE "/FI cstdint")
else()
    set(TRW_CSTDINT_FORCE_INCLUDE "-include cstdint")
endif()
foreach(_trw_rml_target rmlui_core rmlui_debugger recompui)
    if(TARGET ${_trw_rml_target})
        target_compile_options(${_trw_rml_target} PRIVATE "SHELL:$<$<COMPILE_LANGUAGE:CXX>:${TRW_CSTDINT_FORCE_INCLUDE}>")
    endif()
endforeach()

# The libraries call into each other in both directions, so the linker
# rescans them as a group.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    set(TRW_LINK_GROUP_START "-Wl,--start-group")
    set(TRW_LINK_GROUP_END "-Wl,--end-group")
else()
    set(TRW_LINK_GROUP_START "")
    set(TRW_LINK_GROUP_END "")
endif()

target_link_libraries(PaxDino PRIVATE
    ${TRW_LINK_GROUP_START}
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    nfd
    ${TRW_LINK_GROUP_END}
)


# RSP audio microcode: rsp/aspMain.cpp is generated from the ROM by
# RSPRecomp (aspMain.us.toml), like RecompiledFuncs/, so it's gitignored and
# only built when the ROM is present.
set(TRW_ROM_PATH "${CMAKE_SOURCE_DIR}/turokrw.us.z64")
if (EXISTS "${TRW_ROM_PATH}")
    list(REMOVE_ITEM SOURCES ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp)
    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/rsp
        COMMAND $<TARGET_FILE:RSPRecomp> aspMain.us.toml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/aspMain.us.toml RSPRecomp
    )
    target_sources(PaxDino PRIVATE ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp)
    target_compile_definitions(PaxDino PRIVATE TRW_HAS_RSP_AUDIO)
endif()

target_sources(PaxDino PRIVATE ${SOURCES})

set_property(TARGET PaxDino PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
