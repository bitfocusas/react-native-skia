cmake_minimum_required(VERSION 3.21)

project(RNSkiaQtCanvas LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

option(RNSK_QT_BUILD_SMOKE "Build Qt raster canvas smoke test" ON)

# React Native Qt host bridge stays out of standalone QML builds. Set both
# values from host build; no React Native headers or libraries are discovered.
set(RNSK_QT_REACT_NATIVE_INCLUDE_DIRS "" CACHE STRING
    "Semicolon-separated React Native/JSI include directories")
set(RNSK_QT_REACT_NATIVE_LIBRARIES "" CACHE STRING
    "Semicolon-separated React Native/JSI libraries supplied by Qt host")

set(RNSK_QT_SKIA_ROOT "" CACHE PATH
    "Root of external Linux CPU-raster Skia artifact")
set(RNSK_QT_SKIA_SYSTEM_LIBRARIES "" CACHE STRING
    "Extra libraries required by external libskia.a, for example fontconfig;freetype;png;z;dl;pthread")

if(NOT RNSK_QT_SKIA_ROOT)
  message(FATAL_ERROR
    "RNSK_QT_SKIA_ROOT is required. See qt/README.md for external Linux Skia artifact contract.")
endif()

set(RNSK_QT_SKIA_INCLUDE_DIR "${RNSK_QT_SKIA_ROOT}/include")
set(RNSK_QT_SKIA_LIBRARY "${RNSK_QT_SKIA_ROOT}/lib/libskia.a")
if(NOT EXISTS "${RNSK_QT_SKIA_INCLUDE_DIR}/core/SkSurface.h")
  message(FATAL_ERROR
    "Missing ${RNSK_QT_SKIA_INCLUDE_DIR}/core/SkSurface.h in RNSK_QT_SKIA_ROOT.")
endif()
if(NOT EXISTS "${RNSK_QT_SKIA_LIBRARY}")
  message(FATAL_ERROR
    "Missing ${RNSK_QT_SKIA_LIBRARY} in RNSK_QT_SKIA_ROOT.")
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick)

add_library(rnskia_qt_skia STATIC IMPORTED GLOBAL)
set_target_properties(rnskia_qt_skia PROPERTIES
  IMPORTED_LOCATION "${RNSK_QT_SKIA_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${RNSK_QT_SKIA_ROOT}")

add_library(rnskia_qt_canvas STATIC
  RNSkQtCanvasItem.cpp
  RNSkQtCanvasItem.h
  RNSkQtQmlRegistration.cpp
  RNSkQtQmlRegistration.h)
target_compile_features(rnskia_qt_canvas PUBLIC cxx_std_17)
target_include_directories(rnskia_qt_canvas PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rnskia_qt_canvas
  PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick rnskia_qt_skia
         ${RNSK_QT_SKIA_SYSTEM_LIBRARIES})

if(RNSK_QT_REACT_NATIVE_INCLUDE_DIRS OR RNSK_QT_REACT_NATIVE_LIBRARIES)
  if(NOT RNSK_QT_REACT_NATIVE_INCLUDE_DIRS OR NOT RNSK_QT_REACT_NATIVE_LIBRARIES)
    message(FATAL_ERROR
      "Set both RNSK_QT_REACT_NATIVE_INCLUDE_DIRS and RNSK_QT_REACT_NATIVE_LIBRARIES to build rnskia_qt_react_native.")
  endif()

  # Static archive defers React Native ABI resolution to host executable.
  # This keeps QML smoke target independent of React Native Qt installation.
  add_library(rnskia_qt_react_native STATIC
    RNSkQtPlatformContext.cpp
    RNSkQtPlatformContext.h
    RNSkQtManager.cpp
    RNSkQtManager.h
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/rnskia/RNSkManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/jsi/RuntimeAwareCache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/jsi/RuntimeLifecycleMonitor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/jsi/JsiPromises.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/jsi/Promise.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/api/JsiSkDispatcher.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/api/third_party/CSSColorParser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/api/third_party/base64.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/api/third_party/SkottieUtils.cpp")
  # Upstream common RNSkManager uses C++20 Skia APIs.
  target_compile_features(rnskia_qt_react_native PUBLIC cxx_std_20)
  target_include_directories(rnskia_qt_react_native
    PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}"
      ${RNSK_QT_REACT_NATIVE_INCLUDE_DIRS}
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/api"
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/jsi"
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/rnskia"
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpp/utils")
  target_link_libraries(rnskia_qt_react_native
    PUBLIC rnskia_qt_canvas
    PRIVATE ${RNSK_QT_REACT_NATIVE_LIBRARIES})
endif()

if(RNSK_QT_BUILD_SMOKE)
  enable_testing()
  find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
  add_executable(rnskia_qt_canvas_smoke tests/QtCanvasSmokeTest.cpp)
  target_link_libraries(rnskia_qt_canvas_smoke PRIVATE rnskia_qt_canvas Qt6::Test)
  add_test(NAME rnskia_qt_canvas_smoke COMMAND rnskia_qt_canvas_smoke)

  add_executable(rnskia_qt_canvas_qml_integration
    tests/QtCanvasQmlIntegrationTest.cpp)
  target_link_libraries(rnskia_qt_canvas_qml_integration
    PRIVATE rnskia_qt_canvas Qt6::Test)
  add_test(NAME rnskia_qt_canvas_qml_integration
    COMMAND rnskia_qt_canvas_qml_integration)

  set_tests_properties(rnskia_qt_canvas_smoke rnskia_qt_canvas_qml_integration
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software")
endif()
