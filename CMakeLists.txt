cmake_minimum_required(VERSION 3.20)
project(PrimeHost VERSION 0.1.0 LANGUAGES CXX)

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
)

add_library(PrimeHost ${PRIMEHOST_SOURCES})

target_include_directories(PrimeHost
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

ph_require_cxx23(PrimeHost)

option(PRIMEHOST_BUILD_TESTS "Build PrimeHost tests" ON)
if(PRIMEHOST_BUILD_TESTS)
  enable_testing()

  add_executable(PrimeHost_tests
    tests/unit/test_main.cpp
    tests/unit/test_smoke.cpp
  )
  target_link_libraries(PrimeHost_tests PRIVATE PrimeHost)
  target_include_directories(PrimeHost_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  ph_require_cxx23(PrimeHost_tests)
  add_test(NAME PrimeHost_tests COMMAND $<TARGET_FILE:PrimeHost_tests>)
endif()

option(PRIMEHOST_BUILD_EXAMPLES "Build PrimeHost example apps" OFF)
if(PRIMEHOST_BUILD_EXAMPLES)
  add_executable(primehost_stub examples/primehost_stub.cpp)
  target_link_libraries(primehost_stub PRIVATE PrimeHost)
  ph_require_cxx23(primehost_stub)
endif()
