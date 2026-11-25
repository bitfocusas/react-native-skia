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

  // Attaching restores the picture retained by a previous detach of the same
  // nativeId, so a remounted canvas does not go blank until JS resubmits.
  void attachPictureView(size_t nativeId, RNSkQtCanvasItem *item);
  // Unregisters the picture view and retains its last picture for re-attach.
  void detachPictureView(size_t nativeId);
  // Final host release: unregisters and drops any retained picture.
  void releasePictureView(size_t nativeId);

  std::shared_ptr<RNSkManager> skiaManager() const;
  std::shared_ptr<RNSkQtPlatformContext> platformContext() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace RNSkia
