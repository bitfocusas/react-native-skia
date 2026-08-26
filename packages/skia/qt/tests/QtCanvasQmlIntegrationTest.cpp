#include "RNSkQtCanvasItem.h"
#include "RNSkQtQmlRegistration.h"

#include <QFile>
#include <QQuickView>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"

namespace {

bool hasColor(const QImage &image, const QColor &color) {
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y) == color) {
        return true;
      }
    }
  }
  return false;
}

class QtCanvasQmlIntegrationTest final : public QObject {
  Q_OBJECT

private slots:
  void registersQmlTypeAndRendersOffscreen() {
    RNSkia::registerRNSkiaQtTypes();

    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString qmlPath = fixture.filePath("SkiaCanvas.qml");
    QFile qml(qmlPath);
    QVERIFY(qml.open(QIODevice::WriteOnly | QIODevice::Text));
    qml.write("import QtQuick 2.15\n"
              "import org.shopify.RNSkia 1.0\n"
              "SkiaCanvas {\n"
              "  objectName: \"skiaCanvas\"\n"
              "  width: 48\n"
              "  height: 32\n"
              "  nativeId: 1001\n"
              "}\n");
    qml.close();

    QQuickView view;
    view.setColor(Qt::transparent);
    view.setSource(QUrl::fromLocalFile(qmlPath));
    QCOMPARE(view.status(), QQuickView::Ready);

    auto *item = qobject_cast<RNSkia::RNSkQtCanvasItem *>(view.rootObject());
    QVERIFY(item != nullptr);
    QCOMPARE(item->nativeId(), 1001);

    QSignalSpy nativeIdChanged(item, &RNSkia::RNSkQtCanvasItem::nativeIdChanged);
    QSignalSpy nativeIdReleased(item,
                                &RNSkia::RNSkQtCanvasItem::nativeIdReleased);
    QSignalSpy surfaceSizeChanged(
        item, &RNSkia::RNSkQtCanvasItem::surfaceSizeChanged);
    QSignalSpy redrawRequested(item,
                               &RNSkia::RNSkQtCanvasItem::redrawRequested);

    item->setNativeId(1002);
    QCOMPARE(nativeIdChanged.count(), 1);
    QCOMPARE(nativeIdReleased.count(), 1);
    QCOMPARE(nativeIdReleased.at(0).at(0).toInt(), 1001);
    QCOMPARE(nativeIdChanged.at(0).at(0).toInt(), 1002);

    item->setWidth(64);
    item->setHeight(40);
    QVERIFY(surfaceSizeChanged.count() > 0);
    QCOMPARE(item->pixelWidth(), 64);
    QCOMPARE(item->pixelHeight(), 40);

    view.show();
    QTest::qWait(50);
    QVERIFY(item->renderToCanvas([](SkCanvas *canvas) {
      canvas->clear(SK_ColorMAGENTA);
    }));

    QTRY_VERIFY_WITH_TIMEOUT(
        hasColor(view.grabWindow(), QColor::fromRgba(SK_ColorMAGENTA)), 5000);

    item->requestRedraw();
    QCOMPARE(redrawRequested.count(), 1);
    QCOMPARE(redrawRequested.at(0).at(0).toInt(), 1002);

    item->releaseNativeId();
    QCOMPARE(nativeIdReleased.count(), 2);
    QCOMPARE(nativeIdReleased.at(1).at(0).toInt(), 1002);
    QCOMPARE(nativeIdChanged.count(), 2);
    QCOMPARE(nativeIdChanged.at(1).at(0).toInt(), 0);

    item->setNativeId(-1);
    QCOMPARE(item->nativeId(), 0);
    QCOMPARE(nativeIdReleased.count(), 2);
    QCOMPARE(nativeIdChanged.count(), 2);
  }
};

} // namespace

QTEST_MAIN(QtCanvasQmlIntegrationTest)
#include "QtCanvasQmlIntegrationTest.moc"
