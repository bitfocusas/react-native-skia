#pragma once

#include <functional>
#include <memory>
#include <string>

#include "RNSkPlatformContext.h"

namespace RNSkia {

// Qt6 CPU-raster implementation. No GPU, native-buffer, video, or native-view
// screenshot support is provided by this bridge.
class RNSkQtPlatformContext final : public RNSkPlatformContext {
public:
  RNSkQtPlatformContext(
      std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
      float pixelDensity);
  ~RNSkQtPlatformContext() override = default;

  void runOnMainThread(std::function<void()> func) override;
  sk_sp<SkImage> takeScreenshotFromViewTag(size_t tag) override;
  void performStreamOperation(
      const std::string &sourceUri,
      const std::function<void(std::unique_ptr<SkStreamAsset>)> &op) override;
  void raiseError(const std::exception &err) override;
  sk_sp<SkSurface> makeOffscreenSurface(int width, int height,
                                        bool useP3ColorSpace) override;
  sk_sp<SkImage> makeImageFromNativeBuffer(void *buffer) override;

#if !defined(SK_GRAPHITE)
  sk_sp<SkImage> makeImageFromNativeTexture(const TextureInfo &textureInfo,
                                            int width, int height,
                                            bool mipMapped) override;
  const TextureInfo getTexture(sk_sp<SkSurface> surface) override;
  const TextureInfo getTexture(sk_sp<SkImage> image) override;
  GrDirectContext *getDirectContext() override;
#endif

  void releaseNativeBuffer(uint64_t pointer) override;
  uint64_t makeNativeBuffer(sk_sp<SkImage> image) override;
  uint64_t makeTestNativeBuffer(int width, int height) override;
  std::shared_ptr<RNSkVideo> createVideo(const std::string &url) override;
  sk_sp<SkFontMgr> createFontMgr() override;
};

} // namespace RNSkia
