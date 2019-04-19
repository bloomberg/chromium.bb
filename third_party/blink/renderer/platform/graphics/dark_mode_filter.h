#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;

namespace blink {

class DarkModeFilter {
 public:
  DarkModeFilter();

  const DarkModeSettings& settings() const { return settings_; }
  void UpdateSettings(const DarkModeSettings& new_settings);

  sk_sp<SkColorFilter> GetColorFilter();

  bool ShouldApplyToImage(Image& image, const FloatRect& src_rect);

  Color Apply(const Color& color);

 private:
  DarkModeSettings settings_;
  sk_sp<SkColorFilter> default_filter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
