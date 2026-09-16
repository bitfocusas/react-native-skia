#pragma once

#include <functional>
#include <mutex>

#include <QImage>
#include <QQuickItem>

#include "include/core/SkSurface.h"

class QSGNode;
class SkCanvas;

namespace RNSkia {

// CPU-raster counterpart of RNSkCanvasProvider's render callback contract.
class RNSkQtCanvasItem : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(
      int nativeId READ nativeId WRITE setNativeId NOTIFY nativeIdChanged)
  Q_PROPERTY(int pixelWidth READ pixelWidth NOTIFY surfaceSizeChanged)
  Q_PROPERTY(int pixelHeight READ pixelHeight NOTIFY surfaceSizeChanged)

public:
  using DrawCallback = std::function<void(SkCanvas *)>;

  RNSkQtCanvasItem();
  ~RNSkQtCanvasItem() override;

  RNSkQtCanvasItem(const RNSkQtCanvasItem &) = delete;
  RNSkQtCanvasItem &operator=(const RNSkQtCanvasItem &) = delete;

  int nativeId() const;
  void setNativeId(int nativeId);
  int pixelWidth() const;
  int pixelHeight() const;
  bool renderToCanvas(const DrawCallback &draw);
  QImage makeSnapshot() const;

  // Host calls this on detach; destruction releases any remaining id too.
  Q_INVOKABLE void releaseNativeId();
  // QML or host requests a render; host performs it through renderToCanvas().
  Q_INVOKABLE void requestRedraw();

signals:
  void nativeIdChanged(int nativeId);
  void nativeIdReleased(int nativeId);
  void surfaceSizeChanged(int pixelWidth, int pixelHeight);
  void redrawRequested(int nativeId);

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  QSGNode *updatePaintNode(QSGNode *oldNode,
                           UpdatePaintNodeData *data) override;

private:
  bool ensureRasterSurface();
  QImage makeSnapshotLocked() const;
  qreal devicePixelRatio() const;

  // GUI draw/resize and scene-graph snapshot can run on different threads.
  mutable std::mutex rasterMutex_;
  sk_sp<SkSurface> surface_;
  int nativeId_ = 0;
  int pixelWidth_ = 0;
  int pixelHeight_ = 0;
  bool textureDirty_ = true;
};

} // namespace RNSkia
