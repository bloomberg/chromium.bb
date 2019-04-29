#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"

namespace blink {

namespace {

bool ShouldApplyToImage(const DarkModeSettings& settings,
                        const FloatRect& src_rect,
                        Image* image) {
  switch (settings.image_policy) {
    case DarkModeImagePolicy::kFilterSmart:
      return image->ShouldApplyDarkModeFilter(src_rect);
    case DarkModeImagePolicy::kFilterAll:
      return true;
    default:
      return false;
  }
}

}  // namespace

DarkModeFilter::DarkModeFilter()
    : default_filter_(nullptr), image_filter_(nullptr) {
  settings_.mode = DarkMode::kOff;
  settings_.image_policy = DarkModeImagePolicy::kFilterNone;
}

void DarkModeFilter::UpdateSettings(const DarkModeSettings& new_settings) {
  settings_ = new_settings;

  SkHighContrastConfig config;
  switch (settings_.mode) {
    case DarkMode::kOff:
      default_filter_.reset(nullptr);
      image_filter_.reset(nullptr);
      return;
    case DarkMode::kSimpleInvertForTesting: {
      uint8_t identity[256], invert[256];
      for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        invert[i] = 255 - i;
      }
      default_filter_ =
          SkTableColorFilter::MakeARGB(identity, invert, invert, invert);
      image_filter_.reset(nullptr);
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

  if (settings_.image_style == DarkModeImageStyle::kGrayscale) {
    config.fGrayscale = true;
    image_filter_ = SkHighContrastFilter::Make(config);
  } else {
    image_filter_.reset(nullptr);
  }
}

Color DarkModeFilter::ApplyIfNeeded(const Color& color) {
  if (!default_filter_)
    return color;
  return Color(default_filter_->filterColor(color.Rgb()));
}

// TODO(gilmanmh): Investigate making |image| a const reference. This code
// relies on Image::ShouldApplyDarkModeFilter(), which is not const. If it could
// be made const, then |image| could also be const.
void DarkModeFilter::ApplyToImageFlagsIfNeeded(const FloatRect& src_rect,
                                               Image* image,
                                               cc::PaintFlags* flags) {
  sk_sp<SkColorFilter> filter = image_filter_;
  if (!filter)
    filter = default_filter_;

  if (!filter || !ShouldApplyToImage(settings(), src_rect, image))
    return;
  flags->setColorFilter(std::move(filter));
}

base::Optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags) {
  if (!default_filter_)
    return base::nullopt;

  cc::PaintFlags dark_mode_flags = flags;
  if (flags.HasShader()) {
    dark_mode_flags.setColorFilter(default_filter_);
  } else {
    dark_mode_flags.setColor(default_filter_->filterColor(flags.getColor()));
  }

  return base::make_optional<cc::PaintFlags>(std::move(dark_mode_flags));
}

}  // namespace blink
