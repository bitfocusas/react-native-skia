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
  void serializesRepeatedResizeDrawAndSnapshot() {
    RNSkia::RNSkQtCanvasItem item;
    for (int iteration = 1; iteration <= 32; ++iteration) {
      const int width = iteration * 3;
      const int height = iteration * 2;
      item.setWidth(width);
      item.setHeight(height);
      QVERIFY(item.renderToCanvas([iteration](SkCanvas *canvas) {
        canvas->clear(iteration % 2 == 0 ? SK_ColorRED : SK_ColorBLUE);
      }));
      const QImage snapshot = item.makeSnapshot();
      QCOMPARE(snapshot.size(), QSize(width, height));
      QVERIFY(hasNonTransparentPixel(snapshot));
    }
  }

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
