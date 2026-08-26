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

## Deferred integration

This scaffold is **not** React Native component support. Fabric component
descriptor/view registration, TurboModule registration, JSI manager wiring,
JS/TS exports, input/event bridging, lifecycle ownership, and production
packaging remain deferred. Do not register this item as `SkiaPictureView` or
claim React Native rendering support from phase 1.
