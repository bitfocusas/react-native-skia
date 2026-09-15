#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <QGuiApplication>
#include <QTemporaryFile>
#include <QtTest/QTest>

#include <ReactCommon/tests/TestCallInvoker.h>
#include <hermes/hermes.h>

#include "RNSkPictureView.h"
#include "RNSkQtCanvasItem.h"
#include "RNSkQtManager.h"
#include "RNSkQtPlatformContext.h"

namespace RNSkia {

class QtReactNativeBridgeSmokeTest final : public QObject {
  Q_OBJECT

private slots:
  void pictureRendererSynchronizesJsAndGuiPictureAccess() {
    auto renderer = std::make_shared<RNSkPictureRenderer>([] {}, nullptr);
    SkPictureRecorder recorder;
    recorder.beginRecording(4, 4);
    const auto picture = recorder.finishRecordingAsPicture();
    std::atomic<bool> sawNull = false;
    std::vector<std::thread> workers;
    for (int index = 0; index < 4; ++index) {
      workers.emplace_back([renderer, picture, &sawNull]() {
        for (int iteration = 0; iteration < 1000; ++iteration) {
          renderer->setPicture(picture);
          if (renderer->getPicture() == nullptr) {
            sawNull.store(true);
          }
        }
      });
    }
    for (auto &worker : workers) {
      worker.join();
    }
    QVERIFY(!sawNull.load());
  }

  void invalidatedContextDoesNotInvokeDetachedAssetCallback() {
    auto runtime = facebook::hermes::makeHermesRuntime();
    auto callInvoker =
        std::make_shared<facebook::react::TestCallInvoker>(*runtime);
    auto context = std::make_shared<RNSkQtPlatformContext>(callInvoker, 1.0f);
    QTemporaryFile asset;
    QVERIFY(asset.open());
    asset.write("asset");
    asset.flush();
    std::atomic<bool> called = false;
    context->invalidate();
    context->performStreamOperation(asset.fileName().toStdString(),
                                    [&called](std::unique_ptr<SkStreamAsset>) {
                                      called.store(true);
                                    });
    QTest::qWait(100);
    QVERIFY(!called.load());
  }

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
