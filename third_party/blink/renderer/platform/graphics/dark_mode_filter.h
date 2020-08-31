// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_

#include <memory>

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;

namespace blink {

class DarkModeColorClassifier;
class DarkModeColorFilter;
class ScopedDarkModeElementRoleOverride;

class PLATFORM_EXPORT DarkModeFilter {
 public:
  // Dark mode is disabled by default. Enable it by calling UpdateSettings()
  // with a mode other than DarkMode::kOff.
  DarkModeFilter();
  ~DarkModeFilter();

  bool IsDarkModeActive() const;

  const DarkModeSettings& settings() const { return settings_; }
  void UpdateSettings(const DarkModeSettings& new_settings);

  // TODO(gilmanmh): Add a role for shadows. In general, we don't want to
  // invert shadows, but we may need to do some other kind of processing for
  // them.
  enum class ElementRole { kText, kListSymbol, kBackground, kSVG };
  Color InvertColorIfNeeded(const Color& color, ElementRole element_role);
  base::Optional<cc::PaintFlags> ApplyToFlagsIfNeeded(
      const cc::PaintFlags& flags,
      ElementRole element_role);

  // |image| and |flags| must not be null.
  void ApplyToImageFlagsIfNeeded(const FloatRect& src_rect,
                                 const FloatRect& dest_rect,
                                 Image* image,
                                 cc::PaintFlags* flags);

  SkColorFilter* GetImageFilterForTesting() { return image_filter_.get(); }

 private:
  friend class ScopedDarkModeElementRoleOverride;

  DarkModeSettings settings_;

  bool ShouldApplyToColor(const Color& color, ElementRole role);

  std::unique_ptr<DarkModeColorClassifier> text_classifier_;
  std::unique_ptr<DarkModeColorClassifier> background_classifier_;
  std::unique_ptr<DarkModeColorFilter> color_filter_;
  sk_sp<SkColorFilter> image_filter_;
  base::Optional<ElementRole> role_override_;
};

// Temporarily override the element role for the scope of this object's
// lifetime - for example when drawing symbols that play the role of text.
class PLATFORM_EXPORT ScopedDarkModeElementRoleOverride {
 public:
  ScopedDarkModeElementRoleOverride(GraphicsContext* graphics_context,
                                    DarkModeFilter::ElementRole role);
  ~ScopedDarkModeElementRoleOverride();

 private:
  GraphicsContext* graphics_context_;
  base::Optional<DarkModeFilter::ElementRole> previous_role_override_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
