#!/usr/bin/env python3
"""Static contract check usable when external Linux Skia artifact is absent."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(path: Path, text: str) -> None:
    contents = path.read_text()
    assert text in contents, f"{path.relative_to(ROOT)} missing: {text}"


def require_text(name: str, contents: str, text: str) -> None:
    assert text in contents, f"{name} missing: {text}"


def forbid(path: Path, text: str) -> None:
    contents = path.read_text()
    assert text not in contents, f"{path.relative_to(ROOT)} unexpectedly contains: {text}"


def main() -> None:
    cmake = ROOT / "CMakeLists.txt"
    item = ROOT / "RNSkQtCanvasItem.cpp"
    registration = ROOT / "RNSkQtQmlRegistration.cpp"
    registration_header = ROOT / "RNSkQtQmlRegistration.h"
    smoke = ROOT / "tests" / "QtCanvasSmokeTest.cpp"
    qml_integration = ROOT / "tests" / "QtCanvasQmlIntegrationTest.cpp"
    docs = ROOT / "README.md"
    build_script = ROOT / "scripts" / "build-linux-cpu-skia.sh"
    rn_integration_script = ROOT / "scripts" / "run-react-native-qt-integration.sh"
    gn_args = ROOT / "scripts" / "skia-linux-cpu.gn"
    rn_smoke = ROOT / "tests" / "QtReactNativeBridgeSmokeTest.cpp"
    workflow = ROOT.parents[2] / ".github" / "workflows" / "ci.yml"

    require(cmake, "RNSK_QT_SKIA_ROOT")
    require(cmake, "lib/libskia.a")
    require(cmake, "QT_QPA_PLATFORM=offscreen")
    require(cmake, "QSG_RHI_BACKEND=software")
    require(cmake, "RNSK_QT_REACT_NATIVE_INCLUDE_DIRS")
    require(cmake, "RNSK_QT_REACT_NATIVE_LIBRARIES")
    require(cmake, "rnskia_qt_react_native")
    require(cmake, "rnskia_qt_react_native_smoke")
    require(cmake, "RNSK_QT_BUILD_REACT_NATIVE_SMOKE")
    require(cmake, "cxx_std_20")
    require(item, "SkSurfaces::Raster")
    require(item, "QSGSimpleTextureNode")
    require(item, "createTextureFromImage")
    require(item, "nativeIdReleased")
    require(item, "redrawRequested")
    require(registration, "qmlRegisterType<RNSkQtCanvasItem>")
    require(registration_header, "org.shopify.RNSkia")
    forbid(item, "GrDirectContext")
    forbid(item, "GrBackend")
    forbid(item, "Dawn")
    require(smoke, "drawsResizesRedrawsAndSnapshots")
    require(smoke, "hasNonTransparentPixel")
    require(qml_integration, "registersQmlTypeAndRendersOffscreen")
    require(qml_integration, "QQuickView")
    require(qml_integration, "grabWindow")
    require(docs, "registerRNSkiaQtTypes")
    require(docs, "nativeIdReleased")
    require(docs, "Fabric component")
    require(docs, "RNSkQtManager")
    require(docs, "RNSK_QT_REACT_NATIVE_INCLUDE_DIRS")
    require(docs, "attachPictureView")
    require(docs, "run-react-native-qt-integration.sh")
    require(docs, "manifest.json")
    require(docs, "qt-linux-cpu-raster-smoke")
    require(docs, "developer-local images")
    platform_header = ROOT / "RNSkQtPlatformContext.h"
    platform = ROOT / "RNSkQtPlatformContext.cpp"
    manager_header = ROOT / "RNSkQtManager.h"
    manager = ROOT / "RNSkQtManager.cpp"
    require(platform_header, "class RNSkQtPlatformContext")
    require(platform, "QMetaObject::invokeMethod")
    require(platform, "SkSurfaces::Raster")
    require(platform, "SkFontMgr::RefEmpty")
    require(platform, "only supports local")
    require(platform, "unsupported(\"GPU textures\")")
    require(platform, "unsupported(\"native buffers\")")
    require(platform, "unsupported(\"video\")")
    require(manager_header, "attachPictureView")
    require(manager, "std::make_shared<RNSkManager>")
    require(manager, "RNSkPictureView")
    forbid(manager, "Fabric")
    require(build_script, "@sha256:")
    require(build_script, "GN_SHA256")
    require(build_script, "sha256sum --check --status")
    require(build_script, "ctest --test-dir")
    require(build_script, "sync_dep")
    require(build_script, "skparagraph")
    require(build_script, "ADDLIB")
    require(build_script, "--mount")
    require(rn_integration_script, "RNSK_QT_REACT_NATIVE_INCLUDE_DIRS")
    require(rn_integration_script, "rnskia_qt_react_native_smoke")
    require(rn_integration_script, "react-native-qt-qt6-amd64-worktree")
    require(rn_smoke, "makeHermesRuntime")
    require(rn_smoke, "TestCallInvoker")
    require(rn_smoke, "attachPictureView")
    require(build_script, "readonly")
    workflow_contents = workflow.read_text()
    qt_job = workflow_contents.split("  qt-linux-cpu-raster-smoke:\n", 1)[1].split(
        "\n  test-package-e2e:\n", 1
    )[0]
    for text in (
        "runs-on: ubuntu-24.04",
        "submodules: false",
        "git submodule update --init --depth 1 externals/skia",
        'test "$(uname -m)" = x86_64',
        "qt6-base-dev qt6-declarative-dev",
        "python3 bin/fetch-gn",
        "8fa12442a4d0fc4d754e89ec732aa015cdaf8d4bcbbedf68468c652531ea482c",
        "sha256sum --check --status",
        "actions/cache@",
        "runner.arch",
        "steps.skia.outputs.revision",
        "hashFiles('packages/skia/qt/scripts/skia-linux-cpu.gn')",
        "cmake -S packages/skia/qt",
        "ctest --test-dir",
    ):
        require_text("qt-linux-cpu-raster-smoke", qt_job, text)
    assert "docker" not in qt_job.lower(), "qt-linux-cpu-raster-smoke must not use Docker"
    for setting in (
        "skia_enable_ganesh = false",
        "skia_enable_graphite = false",
        "skia_use_gl = false",
        "skia_use_vulkan = false",
        "skia_use_dawn = false",
        "skia_use_angle = false",
        "skia_use_egl = false",
        "skia_use_x11 = false",
        "skia_enable_skottie = true",
        "skia_enable_svg = true",
        "skia_use_harfbuzz = true",
    ):
        require(gn_args, setting)


if __name__ == "__main__":
    main()
