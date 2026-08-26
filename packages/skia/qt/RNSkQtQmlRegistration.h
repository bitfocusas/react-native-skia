#pragma once

namespace RNSkia {

inline constexpr char RNSK_QT_QML_URI[] = "org.shopify.RNSkia";

// Register SkiaCanvas before loading QML that imports RNSK_QT_QML_URI.
// This standalone entry point deliberately has no React Native dependency.
void registerRNSkiaQtTypes();

} // namespace RNSkia
