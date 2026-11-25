#include "RNSkQtManager.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <QMetaObject>
#include <QPointer>

#include "RNSkManager.h"
#include "RNSkPictureView.h"
#include "RNSkQtCanvasItem.h"
#include "RNSkQtPlatformContext.h"

namespace RNSkia {
namespace {

std::shared_ptr<facebook::react::CallInvoker>
requireCallInvoker(std::shared_ptr<facebook::react::CallInvoker> callInvoker) {
  if (!callInvoker) {
    throw std::invalid_argument(
        "RNSkQtManager requires an injected React Native CallInvoker.");
  }
  return callInvoker;
}

float requirePixelDensity(float pixelDensity) {
  if (pixelDensity <= 0.0f) {
    throw std::invalid_argument("RNSkQtManager pixel density must be positive.");
  }
  return pixelDensity;
}

class RNSkQtCanvasProvider final : public RNSkCanvasProvider {
public:
  explicit RNSkQtCanvasProvider(RNSkQtCanvasItem *item)
      : RNSkCanvasProvider([item = QPointer<RNSkQtCanvasItem>(item)]() {
          if (item != nullptr) {
            item->requestRedraw();
          }
        }),
        item_(item) {}

  int getWidth() override { return item_ == nullptr ? 0 : item_->pixelWidth(); }
  int getHeight() override {
    return item_ == nullptr ? 0 : item_->pixelHeight();
  }
  bool renderToCanvas(const std::function<void(SkCanvas *)> &draw) override {
    return item_ != nullptr && item_->renderToCanvas(draw);
  }

private:
  QPointer<RNSkQtCanvasItem> item_;
};

} // namespace

class RNSkQtManager::Impl {
public:
  struct AttachedView {
    QPointer<RNSkQtCanvasItem> item;
    std::shared_ptr<RNSkPictureView> view;
    QMetaObject::Connection releasedConnection;
    QMetaObject::Connection destroyedConnection;
    QMetaObject::Connection resizedConnection;
  };

  Impl(facebook::jsi::Runtime &runtime,
       std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
       float pixelDensity)
      : context(std::make_shared<RNSkQtPlatformContext>(
            requireCallInvoker(jsCallInvoker), requirePixelDensity(pixelDensity))),
        manager(std::make_shared<RNSkManager>(
            &runtime, requireCallInvoker(std::move(jsCallInvoker)), context)) {}

  ~Impl() {
    // Prevent detached asset workers from entering JSI callbacks while manager
    // and Hermes bindings are being released.
    context->invalidate();
    for (const auto &[nativeId, attached] : views) {
      QObject::disconnect(attached.releasedConnection);
      QObject::disconnect(attached.destroyedConnection);
      QObject::disconnect(attached.resizedConnection);
      manager->unregisterSkiaView(nativeId);
    }
  }

  // Removes registry entry and picture view. Returns last submitted picture.
  sk_sp<SkPicture> detach(size_t nativeId) {
    const auto found = views.find(nativeId);
    if (found == views.end()) {
      return nullptr;
    }
    QObject::disconnect(found->second.releasedConnection);
    QObject::disconnect(found->second.destroyedConnection);
    QObject::disconnect(found->second.resizedConnection);
    auto picture = std::static_pointer_cast<RNSkPictureRenderer>(
                       found->second.view->getRenderer())
                       ->getPicture();
    manager->unregisterSkiaView(nativeId);
    views.erase(found);
    return picture;
  }

  std::shared_ptr<RNSkQtPlatformContext> context;
  std::shared_ptr<RNSkManager> manager;
  std::unordered_map<size_t, AttachedView> views;
  // Pictures of detached, not yet released views. Picture payload stays
  // JSI-owned elsewhere; this keeps only the last drawn reference.
  std::unordered_map<size_t, sk_sp<SkPicture>> retainedPictures;
};

RNSkQtManager::RNSkQtManager(
    facebook::jsi::Runtime &runtime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
    float pixelDensity)
    : impl_(std::make_unique<Impl>(runtime, std::move(jsCallInvoker),
                                   pixelDensity)) {}

RNSkQtManager::~RNSkQtManager() = default;

void RNSkQtManager::attachPictureView(size_t nativeId, RNSkQtCanvasItem *item) {
  if (nativeId == 0 || item == nullptr || item->nativeId() != nativeId) {
    throw std::invalid_argument(
        "attachPictureView requires a nonzero nativeId assigned to its Qt item.");
  }
  auto retained = impl_->detach(nativeId);
  if (const auto found = impl_->retainedPictures.find(nativeId);
      found != impl_->retainedPictures.end()) {
    if (retained == nullptr) {
      retained = std::move(found->second);
    }
    impl_->retainedPictures.erase(found);
  }

  auto provider = std::make_shared<RNSkQtCanvasProvider>(item);
  auto view = std::make_shared<RNSkPictureView>(impl_->context, provider);
  view->setNativeId(nativeId);
  if (retained != nullptr) {
    // Restore before registration: pending JS properties applied by
    // registerSkiaView still replace the retained picture.
    std::static_pointer_cast<RNSkPictureRenderer>(view->getRenderer())
        ->setPicture(std::move(retained));
  }
  impl_->manager->registerSkiaView(nativeId, view);

  Impl::AttachedView attached;
  attached.item = item;
  attached.view = std::move(view);
  attached.releasedConnection = QObject::connect(
      item, &RNSkQtCanvasItem::nativeIdReleased, item,
      [this, nativeId](int releasedId) {
        if (releasedId == static_cast<int>(nativeId)) {
          releasePictureView(nativeId);
        }
      });
  attached.destroyedConnection = QObject::connect(
      item, &QObject::destroyed, item,
      [this, nativeId](QObject *) { releasePictureView(nativeId); });
  const auto weakView = std::weak_ptr<RNSkPictureView>(attached.view);
  attached.resizedConnection = QObject::connect(
      item, &RNSkQtCanvasItem::surfaceSizeChanged, item,
      [weakView](int, int) {
        if (const auto view = weakView.lock())
          view->requestRedraw();
      });
  impl_->views.emplace(nativeId, std::move(attached));
}

void RNSkQtManager::detachPictureView(size_t nativeId) {
  if (auto picture = impl_->detach(nativeId); picture != nullptr) {
    impl_->retainedPictures.insert_or_assign(nativeId, std::move(picture));
  } else {
    impl_->retainedPictures.erase(nativeId);
  }
}

void RNSkQtManager::releasePictureView(size_t nativeId) {
  (void)impl_->detach(nativeId);
  impl_->retainedPictures.erase(nativeId);
}

std::shared_ptr<RNSkManager> RNSkQtManager::skiaManager() const {
  return impl_->manager;
}

std::shared_ptr<RNSkQtPlatformContext>
RNSkQtManager::platformContext() const {
  return impl_->context;
}

} // namespace RNSkia
