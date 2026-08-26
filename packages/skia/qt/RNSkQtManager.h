#pragma once

#include <cstddef>
#include <memory>

#include <jsi/jsi.h>

namespace facebook::react {
class CallInvoker;
}

namespace RNSkia {
class RNSkManager;
class RNSkQtCanvasItem;
class RNSkQtPlatformContext;

// Explicit React Native Qt host bridge. Host owns this object and calls attach
// and detach with mounted QML items; it does not discover Fabric views.
class RNSkQtManager final {
public:
  RNSkQtManager(facebook::jsi::Runtime &runtime,
                std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
                float pixelDensity);
  ~RNSkQtManager();

  RNSkQtManager(const RNSkQtManager &) = delete;
  RNSkQtManager &operator=(const RNSkQtManager &) = delete;

  void attachPictureView(size_t nativeId, RNSkQtCanvasItem *item);
  void detachPictureView(size_t nativeId);

  std::shared_ptr<RNSkManager> skiaManager() const;
  std::shared_ptr<RNSkQtPlatformContext> platformContext() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace RNSkia
