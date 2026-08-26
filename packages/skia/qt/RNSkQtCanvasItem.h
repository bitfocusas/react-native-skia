#pragma once

#include <functional>

#include <QImage>
#include <QQuickItem>

#include "include/core/SkSurface.h"

class QSGNode;
class SkCanvas;

namespace RNSkia {

// CPU-raster counterpart of RNSkCanvasProvider's render callback contract.
class RNSkQtCanvasItem final : public QQuickItem {
public:
  using DrawCallback = std::function<void(SkCanvas *)>;

  RNSkQtCanvasItem();
  ~RNSkQtCanvasItem() override;

  RNSkQtCanvasItem(const RNSkQtCanvasItem &) = delete;
  RNSkQtCanvasItem &operator=(const RNSkQtCanvasItem &) = delete;

  int pixelWidth() const;
  int pixelHeight() const;
  bool renderToCanvas(const DrawCallback &draw);
  QImage makeSnapshot() const;

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  QSGNode *updatePaintNode(QSGNode *oldNode,
                           UpdatePaintNodeData *data) override;

private:
  void ensureRasterSurface();
  qreal devicePixelRatio() const;

  sk_sp<SkSurface> surface_;
  int pixelWidth_ = 0;
  int pixelHeight_ = 0;
  bool textureDirty_ = true;
};

} // namespace RNSkia
