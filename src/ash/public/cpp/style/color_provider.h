// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ash {

class ColorModeObserver;

// An interface implemented by Ash that provides colors for the system UI.
class ASH_PUBLIC_EXPORT ColorProvider {
 public:
  // Types of Shield layer. Number at the end of each type indicates the alpha
  // value.
  enum class ShieldLayerType {
    kShield20 = 0,
    kShield40,
    kShield60,
    kShield80,
    kShield90,
    kShield95,
  };

  // Blur sigma for system UI layers.
  static constexpr float kBackgroundBlurSigma = 30.f;

  // The default blur quality for background blur. Using a value less than 1
  // improves performance.
  static constexpr float kBackgroundBlurQuality = 0.33f;

  // Types of Base layer.
  enum class BaseLayerType {
    // Number at the end of each transparent type indicates the alpha value.
    kTransparent20 = 0,
    kTransparent40,
    kTransparent60,
    kTransparent80,
    kTransparent90,
    kTransparent95,

    // Base layer is opaque.
    kOpaque,
  };

  // Types of Controls layer.
  enum class ControlsLayerType {
    kHairlineBorderColor,
    kControlBackgroundColorActive,
    kControlBackgroundColorInactive,
    kControlBackgroundColorAlert,
    kControlBackgroundColorWarning,
    kControlBackgroundColorPositive,
    kFocusAuraColor,
    kFocusRingColor,
    kHighlightColor1,
    kHighlightColor2,
    kBorderColor1,
    kBorderColor2,
  };

  enum class ContentLayerType {
    kScrollBarColor,
    kSeparatorColor,

    kTextColorPrimary,
    kTextColorSecondary,
    kTextColorAlert,
    kTextColorWarning,
    kTextColorPositive,
    kTextColorURL,

    kIconColorPrimary,
    kIconColorSecondary,
    kIconColorAlert,
    kIconColorWarning,
    kIconColorPositive,
    // Color for prominent icon, e.g, "Add connection" icon button inside
    // VPN detailed view.
    kIconColorProminent,

    // Background for kIconColorSecondary.
    kIconColorSecondaryBackground,

    // The default color for button labels.
    kButtonLabelColor,
    kButtonLabelColorPrimary,

    // Color for blue button labels, e.g, 'Retry' button of the system toast.
    kButtonLabelColorBlue,

    kButtonIconColor,
    kButtonIconColorPrimary,

    // Color for app state indicator.
    kAppStateIndicatorColor,
    kAppStateIndicatorColorInactive,

    // Color for the shelf drag handle in tablet mode.
    kShelfHandleColor,

    // Color for slider.
    kSliderColorActive,
    kSliderColorInactive,

    // Color for radio button.
    kRadioColorActive,
    kRadioColorInactive,

    // Color for toggle button.
    kSwitchKnobColorActive,
    kSwitchKnobColorInactive,
    kSwitchTrackColorActive,
    kSwitchTrackColorInactive,

    // Color for current active desk's border.
    kCurrentDeskColor,

    // Color for the battery's badge (bolt, unreliable, X).
    kBatteryBadgeColor,

    // Color for the switch access's back button.
    kSwitchAccessInnerStrokeColor,
    kSwitchAccessOuterStrokeColor,

    // Color for the media controls.
    kProgressBarColorForeground,
    kProgressBarColorBackground,

    // Color used to highlight a hovered view.
    kHighlightColorHover
  };

  static ColorProvider* Get();

  // Gets the color of |type| of the corresponding layer based on the current
  // color mode.
  virtual SkColor GetShieldLayerColor(ShieldLayerType type) const = 0;
  virtual SkColor GetBaseLayerColor(BaseLayerType type) const = 0;
  virtual SkColor GetControlsLayerColor(ControlsLayerType type) const = 0;
  virtual SkColor GetContentLayerColor(ContentLayerType type) const = 0;

  // Gets the active or inactive dialog title bar color in the current color
  // mode.
  virtual SkColor GetActiveDialogTitleBarColor() const = 0;
  virtual SkColor GetInactiveDialogTitleBarColor() const = 0;

  // Gets the ink drop base color and opacity. Since the inkdrop ripple and
  // highlight have the same opacity, we are keeping only one opacity here. The
  // base color will be gotten based on current color mode, which will be WHITE
  // on dark mode and BLACK on light mode. Please provide `background_color` if
  // different base color needed on current color mode. See more details of
  // IsDarkModeEnabled for current color mode.
  virtual std::pair<SkColor, float> GetInkDropBaseColorAndOpacity(
      SkColor background_color = gfx::kPlaceholderColor) const = 0;

  virtual void AddObserver(ColorModeObserver* observer) = 0;
  virtual void RemoveObserver(ColorModeObserver* observer) = 0;

  // True if the current color mode is DARK. The default color mode is LIGHT if
  // the DarkLightMode feature is enabled. And it can be changed through pref
  // `kDarkModeEnabled`. But the default color mode is DARK if the
  // DarkLightMode feature is disabled. And it can be overridden by
  // ScopedLightModeAsDefault. See `override_light_mode_as_default_` for more
  // details.
  virtual bool IsDarkModeEnabled() const = 0;

  // Enable or disable dark mode for testing. Only works when the DarkLightMode
  // feature is enabled.
  virtual void SetDarkModeEnabledForTest(bool enabled) = 0;

 protected:
  ColorProvider();
  virtual ~ColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
