#include <memory>

#include <QGuiApplication>
#include <QtTest/QTest>

#include <ReactCommon/tests/TestCallInvoker.h>
#include <hermes/hermes.h>

#include "RNSkQtCanvasItem.h"
#include "RNSkQtManager.h"
#include "RNSkQtPlatformContext.h"

namespace RNSkia {

class QtReactNativeBridgeSmokeTest final : public QObject {
  Q_OBJECT

private slots:
  void installsBindingsAndAttachesQtCanvas() {
    auto runtime = facebook::hermes::makeHermesRuntime();
    auto callInvoker =
        std::make_shared<facebook::react::TestCallInvoker>(*runtime);
    RNSkQtCanvasItem item;
    item.setWidth(32);
    item.setHeight(16);
    item.setNativeId(42);

    RNSkQtManager manager(*runtime, callInvoker, 1.0f);
    manager.attachPictureView(42, &item);

    QVERIFY(runtime->global().hasProperty(*runtime, "SkiaApi"));
    QVERIFY(runtime->global().hasProperty(*runtime, "SkiaViewApi"));
    QVERIFY(manager.platformContext()->makeOffscreenSurface(8, 4, false));

    manager.detachPictureView(42);
    item.releaseNativeId();
  }
};

} // namespace RNSkia

QTEST_MAIN(RNSkia::QtReactNativeBridgeSmokeTest)
#include "QtReactNativeBridgeSmokeTest.moc"
