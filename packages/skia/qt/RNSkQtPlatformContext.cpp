#include "RNSkQtPlatformContext.h"

#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>
#include <QUrl>
#include <QDebug>

#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"

namespace RNSkia {
namespace {

[[noreturn]] void unsupported(const char *feature) {
  throw std::runtime_error(std::string("Qt CPU-raster Skia bridge does not "
                                      "support ") +
                           feature + ".");
}

QString localPathForUri(const std::string &sourceUri) {
  const QUrl url(QString::fromStdString(sourceUri));
  if (url.scheme().isEmpty()) {
    return QString::fromStdString(sourceUri);
  }
  if (url.isLocalFile()) {
    return url.toLocalFile();
  }
  if (url.scheme() == QLatin1String("qrc")) {
    return QLatin1Char(':') + url.path();
  }
  throw std::runtime_error("Qt CPU-raster Skia bridge only supports local "
                           "file paths, file:// URIs, and qrc:/ assets.");
}

} // namespace

RNSkQtPlatformContext::RNSkQtPlatformContext(
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
    float pixelDensity)
    : RNSkPlatformContext(std::move(jsCallInvoker), pixelDensity) {}

RNSkQtPlatformContext::~RNSkQtPlatformContext() { invalidate(); }

void RNSkQtPlatformContext::invalidate() {
  const auto lifetime = assetOperationLifetime_;
  std::lock_guard<std::mutex> lock(lifetime->mutex);
  lifetime->active = false;
}

void RNSkQtPlatformContext::runOnMainThread(std::function<void()> func) {
  auto *application = QCoreApplication::instance();
  if (application == nullptr) {
    throw std::runtime_error(
        "Qt CPU-raster Skia bridge requires a QCoreApplication for main-thread dispatch.");
  }
  QMetaObject::invokeMethod(application, [func = std::move(func)]() mutable {
    func();
  }, Qt::QueuedConnection);
}

sk_sp<SkImage> RNSkQtPlatformContext::takeScreenshotFromViewTag(size_t) {
  unsupported("native-view screenshots");
}

void RNSkQtPlatformContext::performStreamOperation(
    const std::string &sourceUri,
    const std::function<void(std::unique_ptr<SkStreamAsset>)> &op) {
  const auto lifetime = assetOperationLifetime_;
  std::thread([sourceUri, op, lifetime]() {
    try {
      QFile file(localPathForUri(sourceUri));
      if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Unable to open local Skia asset: " + sourceUri);
      }
      const QByteArray bytes = file.readAll();
      auto data = SkData::MakeWithCopy(bytes.constData(), bytes.size());
      std::lock_guard<std::mutex> lock(lifetime->mutex);
      if (!lifetime->active) {
        return;
      }
      op(SkMemoryStream::Make(std::move(data)));
    } catch (const std::exception &error) {
      qCritical().noquote() << "RNSkia Qt:" << error.what();
    }
  }).detach();
}

void RNSkQtPlatformContext::raiseError(const std::exception &err) {
  qCritical().noquote() << "RNSkia Qt:" << err.what();
}

sk_sp<SkSurface>
RNSkQtPlatformContext::makeOffscreenSurface(int width, int height, bool) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  return SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
}

sk_sp<SkImage> RNSkQtPlatformContext::makeImageFromNativeBuffer(void *) {
  unsupported("native buffers");
}

#if !defined(SK_GRAPHITE)
sk_sp<SkImage> RNSkQtPlatformContext::makeImageFromNativeTexture(
    const TextureInfo &, int, int, bool) {
  unsupported("GPU textures");
}

const TextureInfo RNSkQtPlatformContext::getTexture(sk_sp<SkSurface>) {
  unsupported("GPU textures");
}

const TextureInfo RNSkQtPlatformContext::getTexture(sk_sp<SkImage>) {
  unsupported("GPU textures");
}

GrDirectContext *RNSkQtPlatformContext::getDirectContext() {
  unsupported("GPU contexts");
}
#endif

void RNSkQtPlatformContext::releaseNativeBuffer(uint64_t) {
  unsupported("native buffers");
}

uint64_t RNSkQtPlatformContext::makeNativeBuffer(sk_sp<SkImage>) {
  unsupported("native buffers");
}

uint64_t RNSkQtPlatformContext::makeTestNativeBuffer(int, int) {
  unsupported("native buffers");
}

std::shared_ptr<RNSkVideo>
RNSkQtPlatformContext::createVideo(const std::string &) {
  unsupported("video");
}

sk_sp<SkFontMgr> RNSkQtPlatformContext::createFontMgr() {
  return SkFontMgr::RefEmpty();
}

} // namespace RNSkia
