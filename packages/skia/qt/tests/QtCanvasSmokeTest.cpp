#include "RNSkQtCanvasItem.h"

#include <QGuiApplication>
#include <QtTest/QTest>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"

namespace {

bool hasNonTransparentPixel(const QImage &image) {
  for (int y = 0; y < image.height(); ++y) {
    const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(line[x]) != 0) {
        return true;
      }
    }
  }
  return false;
}

class QtCanvasSmokeTest final : public QObject {
  Q_OBJECT

private slots:
  void drawsResizesRedrawsAndSnapshots() {
    RNSkia::RNSkQtCanvasItem item;
    item.setWidth(32);
    item.setHeight(24);

    QVERIFY(item.renderToCanvas([](SkCanvas *canvas) {
      canvas->clear(SK_ColorTRANSPARENT);
      canvas->drawColor(SK_ColorRED);
    }));
    const QImage initial = item.makeSnapshot();
    QCOMPARE(initial.size(), QSize(32, 24));
    QVERIFY(hasNonTransparentPixel(initial));

    item.setWidth(64);
    item.setHeight(40);
    QCOMPARE(item.pixelWidth(), 64);
    QCOMPARE(item.pixelHeight(), 40);
    QVERIFY(item.renderToCanvas([](SkCanvas *canvas) {
      canvas->clear(SK_ColorTRANSPARENT);
      canvas->drawColor(SK_ColorBLUE);
    }));
    const QImage resized = item.makeSnapshot();
    QCOMPARE(resized.size(), QSize(64, 40));
    QVERIFY(hasNonTransparentPixel(resized));
    QCOMPARE(resized.pixelColor(0, 0), QColor::fromRgba(SK_ColorBLUE));
  }
};

} // namespace

QTEST_MAIN(QtCanvasSmokeTest)
#include "QtCanvasSmokeTest.moc"
