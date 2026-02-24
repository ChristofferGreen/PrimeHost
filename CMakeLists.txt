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
  src/PrimeHostFps.cpp
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
  find_library(AVFOUNDATION_LIBRARY AVFoundation)
  find_library(CARBON_LIBRARY Carbon)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(CORELOCATION_LIBRARY CoreLocation)
  find_library(GAMECONTROLLER_LIBRARY GameController)
  find_library(COREGRAPHICS_LIBRARY CoreGraphics)
  find_library(METAL_LIBRARY Metal)
  find_library(PHOTOS_LIBRARY Photos)
  find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(COREHAPTICS_LIBRARY CoreHaptics)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(QUARTZCORE_LIBRARY QuartzCore)
  find_library(UNIFORMTYPEIDS_LIBRARY UniformTypeIdentifiers)
  find_library(USERNOTIFICATIONS_LIBRARY UserNotifications)
  target_link_libraries(PrimeHost PRIVATE
    ${AVFOUNDATION_LIBRARY}
    ${CARBON_LIBRARY}
    ${COCOA_LIBRARY}
    ${CORELOCATION_LIBRARY}
    ${GAMECONTROLLER_LIBRARY}
    ${COREGRAPHICS_LIBRARY}
    ${METAL_LIBRARY}
    ${PHOTOS_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREHAPTICS_LIBRARY}
    ${IOKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${UNIFORMTYPEIDS_LIBRARY}
    ${USERNOTIFICATIONS_LIBRARY}
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
    tests/unit/test_safe_area.cpp
    tests/unit/test_cursor_visible.cpp
    tests/unit/test_cursor_shape.cpp
    tests/unit/test_cursor_image.cpp
    tests/unit/test_surface_state.cpp
    tests/unit/test_clipboard.cpp
    tests/unit/test_clipboard_size.cpp
    tests/unit/test_clipboard_paths.cpp
    tests/unit/test_clipboard_image.cpp
    tests/unit/test_event_buffer.cpp
    tests/unit/test_event_payload.cpp
    tests/unit/test_event_defaults.cpp
    tests/unit/test_screenshot.cpp
    tests/unit/test_app_paths.cpp
    tests/unit/test_file_dialogs.cpp
    tests/unit/test_surface_scale.cpp
    tests/unit/test_surface_constraints.cpp
    tests/unit/test_surface_caps.cpp
    tests/unit/test_surface_capabilities.cpp
    tests/unit/test_surface_masks.cpp
    tests/unit/test_displays.cpp
    tests/unit/test_display_hdr.cpp
    tests/unit/test_surface_display.cpp
    tests/unit/test_host_extensions.cpp
    tests/unit/test_fps.cpp
    tests/unit/test_power_events.cpp
    tests/unit/test_drop_event.cpp
    tests/unit/test_focus_event.cpp
    tests/unit/test_lifecycle_events.cpp
    tests/unit/test_host_types.cpp
    tests/unit/test_window_icon.cpp
    tests/unit/test_timing.cpp
    tests/unit/test_timing_helpers.cpp
    tests/unit/test_log_callback.cpp
    tests/unit/test_headless_surface.cpp
    tests/unit/test_frame_config_validation.cpp
    tests/unit/test_frame_config_defaults.cpp
    tests/unit/test_frame_config_set.cpp
    tests/unit/test_frame_config_query.cpp
    tests/unit/test_frame_config_util.cpp
    tests/unit/test_frame_diagnostics.cpp
    tests/unit/test_frame_timing.cpp
    tests/unit/test_frame_limiter.cpp
    tests/unit/test_framebuffer.cpp
    tests/unit/test_input_event.cpp
    tests/unit/test_input_frame.mm
    tests/unit/test_resize_frame.cpp
    tests/unit/test_host_limiter_timer.cpp
    tests/unit/test_platform_input_util.cpp
    tests/unit/test_platform_time_util.cpp
    tests/unit/test_focus_frame.mm
    tests/unit/test_platform_display_util.cpp
    tests/unit/test_size_util.cpp
    tests/unit/test_relative_pointer_cursor.mm
    tests/unit/test_gamepad_profiles.cpp
    tests/unit/test_gamepad_ids.cpp
    tests/unit/test_request_frame.cpp
    tests/unit/test_smoke.cpp
    tests/unit/test_wait_events.cpp
    tests/unit/test_ime_rect.cpp
    tests/unit/test_host_callbacks.cpp
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
set(PRIMEHOST_PRIMESTAGE_GIT_REPOSITORY "https://github.com/ChristofferGreen/PrimeStage.git"
    CACHE STRING "PrimeStage git repository")
set(PRIMEHOST_PRIMESTAGE_GIT_TAG "master" CACHE STRING "PrimeStage git tag or commit")
set(PRIMEHOST_PRIMESTUDIO_GIT_REPOSITORY "https://github.com/ChristofferGreen/PrimeStudio.git"
    CACHE STRING "PrimeStudio git repository")
set(PRIMEHOST_PRIMESTUDIO_GIT_TAG "master" CACHE STRING "PrimeStudio git tag or commit")
if(PRIMEHOST_BUILD_EXAMPLES)
  add_executable(primehost_stub examples/primehost_stub.cpp)
  target_link_libraries(primehost_stub PRIVATE PrimeHost)
  ph_require_cxx23(primehost_stub)
  add_executable(primehost_app examples/primehost_app.cpp)
  target_link_libraries(primehost_app PRIVATE PrimeHost)
  ph_require_cxx23(primehost_app)
  add_executable(primehost_gamepad_audio examples/primehost_gamepad_audio.cpp)
  target_link_libraries(primehost_gamepad_audio PRIVATE PrimeHost)
  ph_require_cxx23(primehost_gamepad_audio)
  if(APPLE)
    include(FetchContent)
    set(PRIMESTAGE_FETCH_PRIMEFRAME ON CACHE BOOL "Fetch PrimeFrame dependency" FORCE)
    set(PRIMESTAGE_BUILD_TESTS OFF CACHE BOOL "Build PrimeStage tests" FORCE)
    set(PRIMESTAGE_BUILD_EXAMPLES OFF CACHE BOOL "Build PrimeStage examples" FORCE)
    set(PRIMESTUDIO_FETCH_PRIMESTAGE ON CACHE BOOL "Fetch PrimeStage dependency" FORCE)
    set(PRIMESTUDIO_BUILD_TESTS OFF CACHE BOOL "Build PrimeStudio tests" FORCE)
    set(PRIMESTUDIO_BUILD_EXAMPLES OFF CACHE BOOL "Build PrimeStudio examples" FORCE)
    set(PRIMESTUDIO_ENABLE_PRIMEMANIFEST ON CACHE BOOL "Enable PrimeManifest integration" FORCE)
    set(PRIMESTUDIO_PRIMESTAGE_GIT_REPOSITORY ${PRIMEHOST_PRIMESTAGE_GIT_REPOSITORY}
        CACHE STRING "PrimeStage git repository" FORCE)
    set(PRIMESTUDIO_PRIMESTAGE_GIT_TAG ${PRIMEHOST_PRIMESTAGE_GIT_TAG}
        CACHE STRING "PrimeStage git tag or commit" FORCE)
    FetchContent_Declare(
      PrimeStudio
      GIT_REPOSITORY ${PRIMEHOST_PRIMESTUDIO_GIT_REPOSITORY}
      GIT_TAG ${PRIMEHOST_PRIMESTUDIO_GIT_TAG}
    )
    FetchContent_MakeAvailable(PrimeStudio)

    add_executable(primehost_primestage_demo examples/primehost_primestage_demo.cpp)
    target_link_libraries(primehost_primestage_demo PRIVATE PrimeHost PrimeStudio)
    target_include_directories(primehost_primestage_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    ph_require_cxx23(primehost_primestage_demo)
  endif()
endif()
