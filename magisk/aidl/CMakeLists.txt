# Builds the AIDL effect HAL service.
#
# Not part of the app build: this produces a standalone binary the Magisk module
# installs and init starts. Run generate.sh first to produce the NDK backend
# from the vendored interfaces.
cmake_minimum_required(VERSION 3.22)
project(rv4a_effect_aidl CXX C)

set(CMAKE_CXX_STANDARD 17)
set(GEN ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(ENGINE ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp)

if(NOT EXISTS ${GEN}/include)
    message(FATAL_ERROR "Run magisk/aidl/generate.sh first: ${GEN} is empty")
endif()

file(GLOB_RECURSE AIDL_SRC ${GEN}/src/*.cpp)
# Recursive: the engine is not flat - the convolver and several others sit in
# subdirectories, and a shallow glob leaves their symbols undefined at link.
file(GLOB_RECURSE ENGINE_SRC ${ENGINE}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp/*.c)

# Report what the glob found, and fail early with a clear reason rather than
# letting a short list surface much later as undefined symbols at link - which
# is indistinguishable from a stale build directory when reading a log tail.
list(LENGTH ENGINE_SRC ENGINE_COUNT)
message(STATUS "engine sources found: ${ENGINE_COUNT}")
if(ENGINE_COUNT LESS 40)
    message(FATAL_ERROR
        "Only ${ENGINE_COUNT} engine sources matched - the effects in "
        "jdsp/Effects are missing, so the link would fail on AgcProcess and "
        "friends. Check the ENGINE path: ${ENGINE}")
endif()

add_executable(rv4a-effect-service
        src/service.cpp
        fmq/EventFlag.cpp
        ${AIDL_SRC}
        ${ENGINE_SRC})

target_include_directories(rv4a-effect-service PRIVATE
        ${GEN}/include
        fmq/include
        fmq/base
        fmq/compat            # stand-ins for platform headers absent from the NDK
        compat                # binder service APIs, vendored from AOSP
        ${ENGINE}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
        ${ENGINE}/hal                    # shared parameter dispatch
        ${ENGINE}/libjamesdsp/VeryOldOpen_source_edition/Audio_Engine/eclipse_libjamesdsp_free_bp/jni)  # audio_effect.h

# The engine sources predate current clang defaults, same as in the app build.
target_compile_options(rv4a-effect-service PRIVATE -O2
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-int>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-function-pointer-types>)

find_library(log-lib log)
find_library(binder-lib binder_ndk)
target_link_libraries(rv4a-effect-service ${log-lib} ${binder-lib})

# The NDK's libbinder_ndk provides the app-facing half of the API but omits the
# service registration entry points, since an app has no business publishing a
# system service - which is exactly what a HAL does. Linking a stub instead
# traded one missing set for the other, because both carry the same soname and
# only one can be linked.
#
# So link the real NDK library and let the few platform-only symbols stay
# unresolved: the binary records a dependency on libbinder_ndk.so, and on device
# that resolves to the platform copy, which has all of them.
#
# The cost is that a genuine typo in a symbol name would also link quietly and
# fail at load instead. CI catching the rest is what makes that acceptable, and
# the service logs and exits if registration ever comes back unresolved.
target_link_options(rv4a-effect-service PRIVATE
        -Wl,--unresolved-symbols=ignore-in-object-files)
