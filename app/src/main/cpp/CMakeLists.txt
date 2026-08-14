# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -Ofast -ftree-vectorize -DDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections -Ofast -ftree-vectorize -DDEBUG")

# Declares and names the project.
project(jamesdsp LANGUAGES C)
project(jamesdsp-wrapper LANGUAGES CXX)

include(libcrashlytics-connector/CMakeLists.txt)
include(libjdspimptoolbox/CMakeLists.txt)
include(libjamesdsp-wrapper/CMakeLists.txt)

file(GLOB_RECURSE LIBJAMESDSP_SOURCE_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp/*.c
        ${CMAKE_CURRENT_LIST_DIR}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp/*.h)

add_library( # Sets the name of the library.
        jamesdsp

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${LIBJAMESDSP_SOURCE_FILES}
        EELStdOutExtension.c
        EELStdOutExtension.h)

target_compile_options(jamesdsp PRIVATE -O2)
target_compile_options(jamesdsp PRIVATE -std=gnu11)
target_include_directories(jamesdsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp/)

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

target_link_libraries( # Specifies the target library.
        jamesdsp

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})


# ---------------------------------------------------------------------------
# System-wide effect library for the Magisk module.
#
# Not linked into the APK: this builds the same DSP core as a HAL audio effect
# so a Magisk module can install it into /system/lib*/soundfx, which is what
# makes root mode run this fork's engine rather than the stock one. Excluded
# from the default build so ordinary APK builds are unaffected.
# ---------------------------------------------------------------------------
add_library(
        jamesdsp_hal
        SHARED
        EXCLUDE_FROM_ALL
        ${LIBJAMESDSP_SOURCE_FILES}
        EELStdOutExtension.c
        hal/JamesDspHalEffect.cpp)

# The engine sources predate current clang defaults, which promoted implicit
# declarations and mismatched function pointers to errors. The APK target gets
# past this with -std=gnu11; apply the same to the C sources only, since the
# wrapper itself is C++, and relax the specific checks upstream code trips.
target_compile_options(jamesdsp_hal PRIVATE -O2 -fvisibility=hidden
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-int>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-function-pointer-types>)
target_include_directories(jamesdsp_hal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp/
        ${CMAKE_CURRENT_SOURCE_DIR}/libjamesdsp/VeryOldOpen_source_edition/Audio_Engine/eclipse_libjamesdsp_free_bp/jni/)
# Deliberately NOT named libjamesdsp.so here: that would collide with the APK's
# own target, which emits the same filename. The packaging step renames it when
# copying into the module, where the audio server expects libjamesdsp.so.
target_link_libraries(jamesdsp_hal ${log-lib})
