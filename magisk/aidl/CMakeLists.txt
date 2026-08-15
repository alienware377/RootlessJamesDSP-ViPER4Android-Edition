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
file(GLOB ENGINE_SRC ${ENGINE}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp/*.c)

add_executable(rv4a-effect-service
        src/service.cpp
        src/EffectImpl.cpp
        fmq/EventFlag.cpp
        ${AIDL_SRC}
        ${ENGINE_SRC})

target_include_directories(rv4a-effect-service PRIVATE
        ${GEN}/include
        fmq/include
        fmq/base
        fmq/compat            # stand-ins for platform headers absent from the NDK
        ${ENGINE}/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp)

# The engine sources predate current clang defaults, same as in the app build.
target_compile_options(rv4a-effect-service PRIVATE -O2
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-int>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-function-pointer-types>)

find_library(log-lib log)
find_library(binder-lib binder_ndk)
target_link_libraries(rv4a-effect-service ${log-lib} ${binder-lib})
