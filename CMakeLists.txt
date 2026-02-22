cmake_minimum_required(VERSION 3.20)
if(APPLE)
  project(PrimeHost VERSION 0.1.0 LANGUAGES CXX OBJCXX)
else()
  project(PrimeHost VERSION 0.1.0 LANGUAGES CXX)
endif()

if(APPLE AND CMAKE_OBJCXX_COMPILER_ID STREQUAL "GNU")
  message(FATAL_ERROR "PrimeHost macOS build requires Clang for Objective-C++. Please configure with CMAKE_CXX_COMPILER=clang++ and CMAKE_OBJCXX_COMPILER=clang++.")
endif()

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELEASE_POSTFIX "-release")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-relwithdebinfo")
set(CMAKE_MINSIZEREL_POSTFIX "-minsizerel")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(ph_require_cxx23 target)
  if(CMAKE_CXX_COMPILE_FEATURES)
    target_compile_features(${target} PUBLIC cxx_std_23)
  else()
    message(WARNING "CMake lacks feature metadata for ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}; falling back to CXX_STANDARD on target ${target}.")
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 23)
    set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS OFF)
  endif()
endfunction()

set(PRIMEHOST_SOURCES
  src/PrimeHost.cpp
  src/PrimeHostAudio.cpp
  src/GamepadProfiles.cpp
  src/TextBuffer.h
)

if(APPLE)
  list(APPEND PRIMEHOST_SOURCES
    src/platform/macos/HostMac.mm
    src/platform/macos/AudioMac.mm
  )
endif()

add_library(PrimeHost ${PRIMEHOST_SOURCES})

target_include_directories(PrimeHost
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

ph_require_cxx23(PrimeHost)

if(APPLE)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(GAMECONTROLLER_LIBRARY GameController)
  find_library(COREGRAPHICS_LIBRARY CoreGraphics)
  find_library(METAL_LIBRARY Metal)
  find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(COREHAPTICS_LIBRARY CoreHaptics)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(QUARTZCORE_LIBRARY QuartzCore)
  target_link_libraries(PrimeHost PRIVATE
    ${COCOA_LIBRARY}
    ${GAMECONTROLLER_LIBRARY}
    ${COREGRAPHICS_LIBRARY}
    ${METAL_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREHAPTICS_LIBRARY}
    ${IOKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
  )
  if(CMAKE_OBJCXX_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(src/platform/macos/HostMac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(src/platform/macos/AudioMac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  endif()
endif()

option(PRIMEHOST_BUILD_TESTS "Build PrimeHost tests" ON)
if(PRIMEHOST_BUILD_TESTS)
  enable_testing()

  add_executable(PrimeHost_tests
    tests/unit/test_main.cpp
    tests/unit/test_audio_config_defaults.cpp
    tests/unit/test_audio_config_validation.cpp
    tests/unit/test_audio_smoke.cpp
    tests/unit/test_device_name_match.cpp
    tests/unit/test_devices.cpp
    tests/unit/test_display_interval.cpp
    tests/unit/test_surface_title.cpp
    tests/unit/test_surface_size.cpp
    tests/unit/test_surface_position.cpp
    tests/unit/test_cursor_visible.cpp
    tests/unit/test_surface_state.cpp
    tests/unit/test_clipboard.cpp
    tests/unit/test_frame_config_validation.cpp
    tests/unit/test_frame_config_defaults.cpp
    tests/unit/test_frame_config_query.cpp
    tests/unit/test_frame_config_util.cpp
    tests/unit/test_frame_diagnostics.cpp
    tests/unit/test_frame_limiter.cpp
    tests/unit/test_gamepad_profiles.cpp
    tests/unit/test_gamepad_ids.cpp
    tests/unit/test_smoke.cpp
    tests/unit/test_text_buffer.cpp
  )
  target_link_libraries(PrimeHost_tests PRIVATE PrimeHost)
  target_include_directories(PrimeHost_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  ph_require_cxx23(PrimeHost_tests)
  add_test(NAME PrimeHost_tests COMMAND $<TARGET_FILE:PrimeHost_tests>)
endif()

option(PRIMEHOST_BUILD_EXAMPLES "Build PrimeHost example apps" OFF)
if(PRIMEHOST_BUILD_EXAMPLES)
  add_executable(primehost_stub examples/primehost_stub.cpp)
  target_link_libraries(primehost_stub PRIVATE PrimeHost)
  ph_require_cxx23(primehost_stub)
  add_executable(primehost_gamepad_audio examples/primehost_gamepad_audio.cpp)
  target_link_libraries(primehost_gamepad_audio PRIVATE PrimeHost)
  ph_require_cxx23(primehost_gamepad_audio)
endif()
