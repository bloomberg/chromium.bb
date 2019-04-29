#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"

namespace blink {

DarkModeFilter::DarkModeFilter() : default_filter_(nullptr) {
  settings_.mode = DarkMode::kOff;
  settings_.image_policy = DarkModeImagePolicy::kFilterNone;
}

void DarkModeFilter::UpdateSettings(const DarkModeSettings& new_settings) {
  settings_ = new_settings;

  SkHighContrastConfig config;
  switch (settings_.mode) {
    case DarkMode::kOff:
      default_filter_.reset(nullptr);
      return;
    case DarkMode::kSimpleInvertForTesting: {
      uint8_t identity[256], invert[256];
      for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        invert[i] = 255 - i;
      }
      default_filter_ =
          SkTableColorFilter::MakeARGB(identity, invert, invert, invert);
      return;
    }
    case DarkMode::kInvertBrightness:
      config.fInvertStyle =
          SkHighContrastConfig::InvertStyle::kInvertBrightness;
      break;
    case DarkMode::kInvertLightness:
      config.fInvertStyle = SkHighContrastConfig::InvertStyle::kInvertLightness;
      break;
  }

  config.fGrayscale = settings_.grayscale;
  config.fContrast = settings_.contrast;
  default_filter_ = SkHighContrastFilter::Make(config);
}

sk_sp<SkColorFilter> DarkModeFilter::GetColorFilter() {
  return default_filter_;
}

bool DarkModeFilter::ShouldApplyToImage(Image& image,
                                        const FloatRect& src_rect) {
  if (!GetColorFilter())
    return false;

  switch (settings_.image_policy) {
    case DarkModeImagePolicy::kFilterSmart:
      return image.ShouldApplyDarkModeFilter(src_rect);
    case DarkModeImagePolicy::kFilterAll:
      return true;
    default:
      return false;
  }
}

Color DarkModeFilter::Apply(const Color& color) {
  sk_sp<SkColorFilter> filter = GetColorFilter();
  if (!filter)
    return color;
  return Color(filter->filterColor(color.Rgb()));
}

}  // namespace blink
