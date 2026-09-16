#include "RNSkQtCanvasItem.h"

#include <algorithm>
#include <cmath>

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

namespace RNSkia {

RNSkQtCanvasItem::RNSkQtCanvasItem() {
  setFlag(ItemHasContents, true);
  connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *) {
    if (ensureRasterSurface())
      requestRedraw();
    else
      update();
  });
}

RNSkQtCanvasItem::~RNSkQtCanvasItem() { releaseNativeId(); }

qreal RNSkQtCanvasItem::devicePixelRatio() const {
  return window() == nullptr ? 1.0 : window()->effectiveDevicePixelRatio();
}

int RNSkQtCanvasItem::nativeId() const { return nativeId_; }

void RNSkQtCanvasItem::setNativeId(int nativeId) {
  nativeId = std::max(0, nativeId);
  if (nativeId == nativeId_) {
    return;
  }
  if (nativeId_ != 0) {
    emit nativeIdReleased(nativeId_);
  }
  nativeId_ = nativeId;
  emit nativeIdChanged(nativeId_);
}

int RNSkQtCanvasItem::pixelWidth() const {
  std::lock_guard<std::mutex> lock(rasterMutex_);
  return pixelWidth_;
}

int RNSkQtCanvasItem::pixelHeight() const {
  std::lock_guard<std::mutex> lock(rasterMutex_);
  return pixelHeight_;
}

void RNSkQtCanvasItem::releaseNativeId() { setNativeId(0); }

void RNSkQtCanvasItem::requestRedraw() {
  emit redrawRequested(nativeId_);
  update();
}

bool RNSkQtCanvasItem::ensureRasterSurface() {
  const auto scale = devicePixelRatio();
  const auto width =
      std::max(0, static_cast<int>(std::ceil(this->width() * scale)));
  const auto height =
      std::max(0, static_cast<int>(std::ceil(this->height() * scale)));
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(rasterMutex_);
    if (width == pixelWidth_ && height == pixelHeight_) {
      return false;
    }
    pixelWidth_ = width;
    pixelHeight_ = height;
    surface_ =
        width > 0 && height > 0
            ? SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height))
            : nullptr;
    textureDirty_ = true;
    changed = true;
  }
  if (changed) {
    emit surfaceSizeChanged(width, height);
  }
  return changed;
}

bool RNSkQtCanvasItem::renderToCanvas(const DrawCallback &draw) {
  ensureRasterSurface();
  {
    std::lock_guard<std::mutex> lock(rasterMutex_);
    if (surface_ == nullptr) {
      return false;
    }
    draw(surface_->getCanvas());
    textureDirty_ = true;
  }
  update();
  return true;
}

QImage RNSkQtCanvasItem::makeSnapshotLocked() const {
  if (surface_ == nullptr) {
    return {};
  }
  QImage image(pixelWidth_, pixelHeight_, QImage::Format_ARGB32_Premultiplied);
  if (!surface_->readPixels(
          SkImageInfo::MakeN32Premul(pixelWidth_, pixelHeight_), image.bits(),
          image.bytesPerLine(), 0, 0)) {
    return {};
  }
  image.setDevicePixelRatio(devicePixelRatio());
  return image;
}

QImage RNSkQtCanvasItem::makeSnapshot() const {
  std::lock_guard<std::mutex> lock(rasterMutex_);
  return makeSnapshotLocked();
}

void RNSkQtCanvasItem::geometryChange(const QRectF &newGeometry,
                                      const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  if (ensureRasterSurface())
    requestRedraw();
  else
    update();
}

QSGNode *RNSkQtCanvasItem::updatePaintNode(QSGNode *oldNode,
                                           UpdatePaintNodeData *) {
  auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
  if (window() == nullptr) {
    delete node;
    return nullptr;
  }

  QImage snapshot;
  bool textureDirty = false;
  {
    std::lock_guard<std::mutex> lock(rasterMutex_);
    if (surface_ == nullptr) {
      delete node;
      return nullptr;
    }
    textureDirty = textureDirty_;
    if (textureDirty) {
      snapshot = makeSnapshotLocked();
      textureDirty_ = false;
    }
  }

  if (node == nullptr) {
    node = new QSGSimpleTextureNode();
    node->setOwnsTexture(true);
  }
  if (textureDirty) {
    node->setTexture(window()->createTextureFromImage(snapshot));
  }
  node->setRect(boundingRect());
  return node;
}

} // namespace RNSkia
