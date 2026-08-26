cmake_minimum_required(VERSION 3.21)

project(RNSkiaQtCanvas LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

option(RNSK_QT_BUILD_SMOKE "Build Qt raster canvas smoke test" ON)

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

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Quick)

add_library(rnskia_qt_skia STATIC IMPORTED GLOBAL)
set_target_properties(rnskia_qt_skia PROPERTIES
  IMPORTED_LOCATION "${RNSK_QT_SKIA_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${RNSK_QT_SKIA_ROOT}")

add_library(rnskia_qt_canvas STATIC
  RNSkQtCanvasItem.cpp
  RNSkQtCanvasItem.h)
target_compile_features(rnskia_qt_canvas PUBLIC cxx_std_17)
target_include_directories(rnskia_qt_canvas PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rnskia_qt_canvas
  PUBLIC Qt6::Core Qt6::Gui Qt6::Quick rnskia_qt_skia
         ${RNSK_QT_SKIA_SYSTEM_LIBRARIES})

if(RNSK_QT_BUILD_SMOKE)
  enable_testing()
  find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
  add_executable(rnskia_qt_canvas_smoke tests/QtCanvasSmokeTest.cpp)
  target_link_libraries(rnskia_qt_canvas_smoke PRIVATE rnskia_qt_canvas Qt6::Test)
  add_test(NAME rnskia_qt_canvas_smoke COMMAND rnskia_qt_canvas_smoke)
  set_tests_properties(rnskia_qt_canvas_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software")
endif()
