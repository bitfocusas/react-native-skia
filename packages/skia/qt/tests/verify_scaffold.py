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


if __name__ == "__main__":
    main()
