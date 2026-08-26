# Qt6 CPU Raster Canvas Smoke Scaffold

Phase 1 proves Qt6 can display CPU-raster Skia output through `QQuickItem` and
`QSGTexture`. `RNSkQtCanvasItem` keeps an in-memory raster `SkSurface`; each
draw copies a `QImage` snapshot into a scene-graph texture. Skia does not create
or use Ganesh, Graphite, OpenGL, Vulkan, Metal, or Dawn contexts here.

`RNSkQtCanvasItem::DrawCallback` matches common `RNSkCanvasProvider` callback
shape (`std::function<void(SkCanvas *)>`), without pulling React Native/JSI
headers into this standalone Qt smoke target.

## External Linux Skia artifact contract

Artifacts are external and intentionally not committed. Build or provide a
Linux x86_64 Skia CPU-raster artifact with this layout:

```text
$RNSK_QT_SKIA_ROOT/
  include/core/SkCanvas.h
  include/core/SkImageInfo.h
  include/core/SkSurface.h
  lib/libskia.a
```

Build artifact with CPU raster enabled and every GPU backend disabled. It must
match Qt container C++ ABI and include headers from same Skia revision as
`libskia.a`. Supply static-library transitive dependencies with
`RNSK_QT_SKIA_SYSTEM_LIBRARIES`; exact set depends on Skia build, commonly
`fontconfig;freetype;png;z;dl;pthread`.

## Build and run smoke test

```sh
cmake -S packages/skia/qt -B /tmp/rnskia-qt-build \
  -DRNSK_QT_SKIA_ROOT=/opt/rnskia-skia-linux \
  -DRNSK_QT_SKIA_SYSTEM_LIBRARIES='fontconfig;freetype;png;z;dl;pthread'
cmake --build /tmp/rnskia-qt-build
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  ctest --test-dir /tmp/rnskia-qt-build --output-on-failure
```

Smoke test draws opaque pixels, resizes CPU raster surface, redraws, and checks
snapshot dimensions plus pixels. `QSG_RHI_BACKEND=software` keeps Qt headless
scene graph on software backend; no Skia GPU path exists in target.

## Deferred integration

This scaffold is **not** React Native component support. Fabric component
descriptor/view registration, TurboModule registration, JSI manager wiring,
JS/TS exports, input/event bridging, lifecycle ownership, and production
packaging remain deferred. Do not register this item as `SkiaPictureView` or
claim React Native rendering support from phase 1.
