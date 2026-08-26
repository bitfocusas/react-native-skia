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
    ensureRasterSurface();
    update();
  });
}

RNSkQtCanvasItem::~RNSkQtCanvasItem() {
  releaseNativeId();
}

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

int RNSkQtCanvasItem::pixelWidth() const { return pixelWidth_; }

int RNSkQtCanvasItem::pixelHeight() const { return pixelHeight_; }

void RNSkQtCanvasItem::releaseNativeId() {
  setNativeId(0);
}

void RNSkQtCanvasItem::requestRedraw() {
  emit redrawRequested(nativeId_);
  update();
}

void RNSkQtCanvasItem::ensureRasterSurface() {
  const auto scale = devicePixelRatio();
  const auto width = std::max(0, static_cast<int>(std::ceil(this->width() * scale)));
  const auto height =
      std::max(0, static_cast<int>(std::ceil(this->height() * scale)));
  if (width == pixelWidth_ && height == pixelHeight_) {
    return;
  }

  pixelWidth_ = width;
  pixelHeight_ = height;
  surface_ = width > 0 && height > 0
      ? SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height))
      : nullptr;
  textureDirty_ = true;
  emit surfaceSizeChanged(pixelWidth_, pixelHeight_);
}

bool RNSkQtCanvasItem::renderToCanvas(const DrawCallback &draw) {
  ensureRasterSurface();
  if (surface_ == nullptr) {
    return false;
  }
  draw(surface_->getCanvas());
  textureDirty_ = true;
  update();
  return true;
}

QImage RNSkQtCanvasItem::makeSnapshot() const {
  if (surface_ == nullptr) {
    return {};
  }

  QImage image(pixelWidth_, pixelHeight_, QImage::Format_ARGB32_Premultiplied);
  if (!surface_->readPixels(SkImageInfo::MakeN32Premul(pixelWidth_, pixelHeight_),
                            image.bits(), image.bytesPerLine(), 0, 0)) {
    return {};
  }
  image.setDevicePixelRatio(devicePixelRatio());
  return image;
}

void RNSkQtCanvasItem::geometryChange(const QRectF &newGeometry,
                                      const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  ensureRasterSurface();
  update();
}

QSGNode *RNSkQtCanvasItem::updatePaintNode(QSGNode *oldNode,
                                           UpdatePaintNodeData *) {
  auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
  if (surface_ == nullptr || window() == nullptr) {
    delete node;
    return nullptr;
  }

  if (node == nullptr) {
    node = new QSGSimpleTextureNode();
    node->setOwnsTexture(true);
  }
  if (textureDirty_) {
    node->setTexture(window()->createTextureFromImage(makeSnapshot()));
    textureDirty_ = false;
  }
  node->setRect(boundingRect());
  return node;
}

} // namespace RNSkia
