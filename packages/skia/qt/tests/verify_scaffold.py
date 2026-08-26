#!/usr/bin/env python3
"""Static contract check usable when external Linux Skia artifact is absent."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(path: Path, text: str) -> None:
    contents = path.read_text()
    assert text in contents, f"{path.relative_to(ROOT)} missing: {text}"


def forbid(path: Path, text: str) -> None:
    contents = path.read_text()
    assert text not in contents, f"{path.relative_to(ROOT)} unexpectedly contains: {text}"


def main() -> None:
    cmake = ROOT / "CMakeLists.txt"
    item = ROOT / "RNSkQtCanvasItem.cpp"
    smoke = ROOT / "tests" / "QtCanvasSmokeTest.cpp"
    docs = ROOT / "README.md"
    build_script = ROOT / "scripts" / "build-linux-cpu-skia.sh"
    gn_args = ROOT / "scripts" / "skia-linux-cpu.gn"

    require(cmake, "RNSK_QT_SKIA_ROOT")
    require(cmake, "lib/libskia.a")
    require(cmake, "QT_QPA_PLATFORM=offscreen")
    require(cmake, "QSG_RHI_BACKEND=software")
    require(item, "SkSurfaces::Raster")
    require(item, "QSGSimpleTextureNode")
    require(item, "createTextureFromImage")
    forbid(item, "GrDirectContext")
    forbid(item, "GrBackend")
    forbid(item, "Dawn")
    require(smoke, "drawsResizesRedrawsAndSnapshots")
    require(smoke, "hasNonTransparentPixel")
    require(docs, "Fabric component")
    require(docs, "JSI manager wiring")
    require(docs, "manifest.json")
    require(build_script, "@sha256:")
    require(build_script, "GN_SHA256")
    require(build_script, "sha256sum --check --status")
    require(build_script, "ctest --test-dir")
    require(build_script, "--mount")
    require(build_script, "readonly")
    for setting in (
        "skia_enable_ganesh = false",
        "skia_enable_graphite = false",
        "skia_use_gl = false",
        "skia_use_vulkan = false",
        "skia_use_dawn = false",
        "skia_use_angle = false",
        "skia_use_egl = false",
        "skia_use_x11 = false",
    ):
        require(gn_args, setting)


if __name__ == "__main__":
    main()
