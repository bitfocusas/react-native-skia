# Qt6 CPU Raster Canvas Host Bridge

CPU-raster Skia output reaches Qt6 through `QQuickItem` and `QSGTexture`.
`RNSkQtCanvasItem` keeps an in-memory raster `SkSurface`; each draw copies a
`QImage` snapshot into a scene-graph texture. Skia does not create or use
Ganesh, Graphite, OpenGL, Vulkan, Metal, or Dawn contexts here.

`RNSkQtCanvasItem::DrawCallback` matches common `RNSkCanvasProvider` callback
shape (`std::function<void(SkCanvas *)>`), without pulling React Native/JSI
headers into standalone Qt target.

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
`RNSK_QT_SKIA_SYSTEM_LIBRARIES` only when providing a different artifact.

## Reproducible Linux artifact and smoke test

`qt-linux-cpu-raster-smoke` CI gate builds this contract from locked
`externals/skia` on Ubuntu amd64, then configures, builds, and CTests Qt6 smoke
target. It initializes only Skia submodule, installs Qt6/CMake/Ninja/GCC build
dependencies, and bootstraps Skia-pinned GN with checksum verification. It does
not use Docker or developer-local images. Skia build output cache key includes
runner OS/architecture, locked Skia revision, and `skia-linux-cpu.gn` hash; no
prefix fallback restores incompatible output. CI artifact is temporary and is
not published.

`qt/scripts/build-linux-cpu-skia.sh` remains local container harness. It copies
source into container scratch space; source checkout stays read-only. Output
path must not exist. Artifacts stay external.

```sh
git submodule update --init externals/skia externals/depot_tools
packages/skia/qt/scripts/build-linux-cpu-skia.sh /tmp/rnskia-skia-linux
```

Script requires immutable image reference. Default is pinned local Qt toolchain
image `react-native-qt@sha256:88a75994099e6e91034fd29def5a2cf719aa5d5d8254e7d1af9b14c8f4dee8a6`.
Override only with digest, never mutable tag:

```sh
RNSK_QT_DOCKER_IMAGE='registry.example/qt6@sha256:...' \
  packages/skia/qt/scripts/build-linux-cpu-skia.sh /tmp/rnskia-skia-linux
```

Build uses `skia-linux-cpu.gn`: Ganesh, Graphite, GL, Vulkan, Dawn, ANGLE,
EGL, X11, codecs, and system font backends are disabled. Bare `depot_tools`
GN wrapper expects a separately bootstrapped GN binary, so script uses locked
Skia checkout's `bin/fetch-gn`; downloaded pinned GN binary is SHA-256 checked
before execution. Result contains
`skia-args.gn` and `manifest.json` with locked Skia revision, image digest, GN
checksum, and artifact checksums. Keep manifest with artifact; verify it before
external distribution.

To test a separately supplied artifact:

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

## Qt host registration and lifecycle bridge

Qt hosts call `RNSkia::registerRNSkiaQtTypes()` before loading QML. It registers
one reusable QML type: `SkiaCanvas` in `org.shopify.RNSkia 1.0`.

```qml
import QtQuick 2.15
import org.shopify.RNSkia 1.0

SkiaCanvas {
  nativeId: 1001
  width: 320
  height: 180
}
```

`nativeId` is nonnegative integer host identity, matching numeric identity used
by `RNSkView`/`RNSkManager`; it is not QML object ID or React `nativeID` string.
Host must connect lifecycle signals before assigning ID:

- `nativeIdChanged(id)`: ID assigned, replaced, or cleared (`0`).
- `nativeIdReleased(id)`: previous nonzero ID must be detached. Emitted before
  replacement, `releaseNativeId()`, and item destruction.
- `surfaceSizeChanged(pixelWidth, pixelHeight)`: CPU surface resized after Qt
  geometry/device-pixel-ratio change.
- `redrawRequested(id)`: emitted by `requestRedraw()`. Host renders through
  `renderToCanvas()` then Qt schedules scene-graph update.

`releaseNativeId()` is explicit unmount path. `RNSkQtCanvasItem` also releases
remaining ID during destruction. `renderToCanvas()` remains native C++ API;
QML cannot supply arbitrary Skia callback. This lets React Native Qt host map
its own mounted view lifecycle, renderer, and manager onto item without this
library inventing competing registry.

`rnskia_qt_canvas_qml_integration` loads `SkiaCanvas` from QML through explicit
registration, exercises native-ID replacement/release, resize/redraw signals,
and verifies Skia frame via offscreen `QQuickView::grabWindow()`.

## React Native Qt host bridge (explicit opt-in)

Standalone QML target still has no React Native dependency. Build bridge only
when host supplies **both** explicit parameters; CMake does not find, fetch, or
invent React Native Qt/Fabric dependencies:

```sh
cmake -S packages/skia/qt -B /tmp/rnskia-qt-rn \
  -DRNSK_QT_SKIA_ROOT=/opt/rnskia-skia-linux \
  -DRNSK_QT_REACT_NATIVE_INCLUDE_DIRS='/opt/rn/ReactCommon/callinvoker;/opt/rn/ReactCommon/jsi;/opt/rn/ReactCommon;/opt/rn/ReactCommon/react/nativemodule/core;/opt/rn/ReactCommon/runtimeexecutor' \
  -DRNSK_QT_REACT_NATIVE_LIBRARIES='reactnative;jsi;react_utils'
cmake --build /tmp/rnskia-qt-rn --target rnskia_qt_react_native
```

`RNSkQtManager` is explicit host glue. Construct it on injected JSI runtime's
thread, with React Native Qt's real `CallInvoker`; manager installs upstream
`RNSkManager` JSI APIs on that runtime. No Fabric discovery, automatic
component registration, or fake call invoker exists.

```cpp
#include "RNSkQtManager.h"

// Host owns runtime, callInvoker, and manager lifetime. density comes from
// mounted item's QQuickWindow, for example window->effectiveDevicePixelRatio().
auto skiaManager = std::make_unique<RNSkia::RNSkQtManager>(
    runtime, callInvoker, item->window()->effectiveDevicePixelRatio());

item->setNativeId(1001); // Host mount identity, before attachment.
skiaManager->attachPictureView(1001, item);

// Host unmount path. Item nativeIdReleased also unregisters this mapping.
skiaManager->detachPictureView(1001);
item->releaseNativeId();
```

Attach each mounted `RNSkQtCanvasItem` after assigning its nonzero `nativeId`.
On native-ID replacement, item first emits `nativeIdReleased`; host assigns new
ID then calls `attachPictureView(newId, item)`. `RNSkQtManager` creates common
`RNSkPictureView`/`RNSkManager` bindings; Qt host remains owner of mount,
layout, Fabric descriptor/view registration, and call-invoker lifetime.

`RNSkQtPlatformContext` supports queued Qt main-thread dispatch, local plain
paths, `file://`, and `qrc:/` streams, `qCritical` error logging, CPU raster
offscreen surfaces, and Skia empty custom font manager (font bytes remain usable
through upstream APIs). Native-view screenshots, video, native buffers, GPU
textures, and GPU contexts throw explicit unsupported errors. HTTP/network URIs
are not loaded.

Fabric component descriptor/view registration, TurboModule registration, JS/TS
exports, input/event bridging, React lifecycle ownership, and production
packaging remain host responsibilities. Do not register QML `SkiaCanvas` as
`SkiaPictureView`; this bridge only attaches host-provided mounted items.
