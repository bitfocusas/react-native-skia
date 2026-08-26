#include "RNSkQtQmlRegistration.h"

#include <mutex>

#include <qqml.h>

#include "RNSkQtCanvasItem.h"

namespace RNSkia {

void registerRNSkiaQtTypes() {
  static std::once_flag registered;
  std::call_once(registered, []() {
    qmlRegisterType<RNSkQtCanvasItem>(RNSK_QT_QML_URI, 1, 0, "SkiaCanvas");
  });
}

} // namespace RNSkia
