# Builds the RV4A Bass Exciter as a VST3 plugin.
#
# VST3 rather than VST2: the VST2 SDK has not been distributable for years, and
# FL Studio has supported VST3 since well before 20.
cmake_minimum_required(VERSION 3.20)
project(RV4ABassExciter VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# The SDK needs its submodules (base, pluginterfaces, public.sdk); without them
# the CMake helpers below simply do not exist.
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.7.9_build_61
    GIT_SUBMODULES_RECURSE ON
    GIT_SHALLOW    ON)

set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)   # no editor, no VSTGUI
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(vst3sdk)

smtg_enable_vst3_sdk()

# SingleComponentEffect is not part of the `sdk` link target, so it is compiled
# in directly - without it the build compiles cleanly then fails at link with
# every method unresolved. Only this one file: it pulls in the edit controller
# itself, so adding that separately defines everything twice instead.
set(SDK_EXTRA ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/vstsinglecomponenteffect.cpp)

# One bundle per effect rather than one plugin with a mode switch: hosts list
# them separately, and a user reaching for "dynamic bass" should not have to
# know it lives inside something called an exciter.
smtg_add_vst3plugin(RV4ABassExciter src/plugin.cpp src/BassExciterDsp.h ${SDK_EXTRA})
target_include_directories(RV4ABassExciter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(RV4ABassExciter PRIVATE sdk)

smtg_add_vst3plugin(RV4ADynamicSystem src/plugin_dynamicbass.cpp src/DynamicBassDsp.h ${SDK_EXTRA})
target_include_directories(RV4ADynamicSystem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(RV4ADynamicSystem PRIVATE sdk)

# The distortion is compiled from the app's own source, unmodified, behind a
# small shim - so it cannot drift from the app as the effect is developed.
smtg_add_vst3plugin(RV4AMultibandDistortion
    src/plugin_multiband.cpp
    src/engine/multibandDist.c
    src/engine/digitalFilters.c
    src/engine/jdsp_lock.c
    ${SDK_EXTRA})
target_include_directories(RV4AMultibandDistortion PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/engine)
target_link_libraries(RV4AMultibandDistortion PRIVATE sdk)
