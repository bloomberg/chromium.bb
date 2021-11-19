// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {
// Constants for painting overlay scrollbars. Other properties needed outside
// this painting code are defined in overlay_scrollbar_constants_aura.h.
constexpr int kOverlayScrollbarMinimumLength = 32;
// 2 pixel border with 1 pixel center patch. The border is 2 pixels despite the
// stroke width being 1 so that the inner pixel can match the center tile
// color. This prevents color interpolation between the patches.
constexpr int kOverlayScrollbarBorderPatchWidth = 2;
constexpr int kOverlayScrollbarCenterPatchSize = 1;
const SkScalar kScrollRadius =
    1;  // select[multiple] radius+width are set in css
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTheme:

#if !defined(OS_APPLE)
// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeAura::web_instance();
}

#if !defined(OS_WIN)
// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  static base::NoDestructor<NativeThemeAura> s_native_theme(false, false);
  return s_native_theme.get();
}

NativeTheme* NativeTheme::GetInstanceForDarkUI() {
  static base::NoDestructor<NativeThemeAura> s_native_theme(false, true);
  return s_native_theme.get();
}
#endif  // !OS_WIN
#endif  // !OS_APPLE

////////////////////////////////////////////////////////////////////////////////
// NativeThemeAura:

NativeThemeAura::NativeThemeAura(bool use_overlay_scrollbars,
                                 bool should_only_use_dark_colors)
    : NativeThemeBase(should_only_use_dark_colors),
      use_overlay_scrollbars_(use_overlay_scrollbars) {
// We don't draw scrollbar buttons.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  set_scrollbar_button_length(0);
#endif

  if (use_overlay_scrollbars_) {
    scrollbar_width_ =
        kOverlayScrollbarThumbWidthPressed + kOverlayScrollbarStrokeWidth;
  }

  // Images and alphas declarations assume the following order.
  static_assert(kDisabled == 0, "states unexpectedly changed");
  static_assert(kHovered == 1, "states unexpectedly changed");
  static_assert(kNormal == 2, "states unexpectedly changed");
  static_assert(kPressed == 3, "states unexpectedly changed");
}

NativeThemeAura::~NativeThemeAura() {}

// static
NativeThemeAura* NativeThemeAura::web_instance() {
  static base::NoDestructor<NativeThemeAura> s_native_theme_for_web(
      IsOverlayScrollbarEnabled(), false);
  return s_native_theme_for_web.get();
}

SkColor NativeThemeAura::FocusRingColorForBaseColor(SkColor base_color) const {
#if defined(OS_APPLE)
  // On Mac OSX, the system Accent Color setting is darkened a bit
  // for better contrast.
  return SkColorSetA(base_color, 166);
#else
  return base_color;
#endif  // OS_APPLE
}

void NativeThemeAura::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  SkColor color =
      GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor, color_scheme);
  if (menu_background.corner_radius > 0) {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(color);

    SkPath path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, flags);
  } else {
    canvas->drawColor(color, SkBlendMode::kSrc);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  CommonThemePaintMenuItemBackground(this, canvas, state, rect, menu_item,
                                     color_scheme);
}

void NativeThemeAura::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  SkColor bg_color = GetControlColor(kScrollbarArrowBackground, color_scheme);
  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = gfx::kPlaceholderColor;
  switch (state) {
    case kDisabled:
      arrow_color = GetArrowColor(state, color_scheme);
      break;
    case kHovered:
      bg_color =
          GetControlColor(kScrollbarArrowBackgroundHovered, color_scheme);
      arrow_color = GetControlColor(kScrollbarArrowHovered, color_scheme);
      break;
    case kNormal:
      arrow_color = GetControlColor(kScrollbarArrow, color_scheme);
      break;
    case kPressed:
      bg_color =
          GetControlColor(kScrollbarArrowBackgroundPressed, color_scheme);
      arrow_color = GetControlColor(kScrollbarArrowPressed, color_scheme);
      break;
    case kNumStates:
      break;
  }
  DCHECK_NE(arrow_color, gfx::kPlaceholderColor);

  cc::PaintFlags flags;
  flags.setColor(bg_color);

  SkScalar upper_left_radius = 0;
  SkScalar lower_left_radius = 0;
  SkScalar upper_right_radius = 0;
  SkScalar lower_right_radius = 0;
  float zoom = arrow.zoom ? arrow.zoom : 1.0;
  if (direction == kScrollbarUpArrow) {
    if (arrow.right_to_left) {
      upper_left_radius = kScrollRadius * zoom;
    } else {
      upper_right_radius = kScrollRadius * zoom;
    }
  } else if (direction == kScrollbarDownArrow) {
    if (arrow.right_to_left) {
      lower_left_radius = kScrollRadius * zoom;
    } else {
      lower_right_radius = kScrollRadius * zoom;
    }
  }
  DrawPartiallyRoundRect(canvas, rect, upper_left_radius, upper_right_radius,
                         lower_right_radius, lower_left_radius, flags);

  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  SkColor track_color = GetControlColor(kScrollbarTrack, color_scheme);
  flags.setColor(track_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeAura::PaintScrollbarThumb(cc::PaintCanvas* canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect,
                                          ScrollbarOverlayColorTheme theme,
                                          ColorScheme color_scheme) const {
  // Do not paint if state is disabled.
  if (state == kDisabled)
    return;

  TRACE_EVENT0("blink", "NativeThemeAura::PaintScrollbarThumb");

  gfx::Rect thumb_rect(rect);
  SkColor thumb_color;

  if (use_overlay_scrollbars_) {
    if (state == NativeTheme::kDisabled)
      return;

    const bool hovered = state != kNormal;
    if (color_scheme != ColorScheme::kPlatformHighContrast) {
      // A light system theme uses a dark overlay scrollbar, and vice versa.
      color_scheme = (theme == ScrollbarOverlayColorThemeLight)
                         ? ColorScheme::kDark
                         : ColorScheme::kLight;
    }
    thumb_color =
        GetSystemColor(hovered ? kColorId_OverlayScrollbarThumbHoveredFill
                               : kColorId_OverlayScrollbarThumbFill,
                       color_scheme);
    SkColor stroke_color =
        GetSystemColor(hovered ? kColorId_OverlayScrollbarThumbHoveredStroke
                               : kColorId_OverlayScrollbarThumbStroke,
                       color_scheme);

    // In overlay mode, draw a stroke (border).
    constexpr int kStrokeWidth = kOverlayScrollbarStrokeWidth;
    cc::PaintFlags flags;
    flags.setColor(stroke_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kStrokeWidth);

    gfx::RectF stroke_rect(thumb_rect);
    gfx::InsetsF stroke_insets(kStrokeWidth / 2.f);
    // The edge to which the scrollbar is attached shouldn't have a border.
    gfx::Insets edge_adjust_insets;
    if (part == NativeTheme::kScrollbarHorizontalThumb)
      edge_adjust_insets = gfx::Insets(0, 0, -kStrokeWidth, 0);
    else
      edge_adjust_insets = gfx::Insets(0, 0, 0, -kStrokeWidth);
    stroke_rect.Inset(stroke_insets + edge_adjust_insets);
    canvas->drawRect(gfx::RectFToSkRect(stroke_rect), flags);

    // Inset the all the edges edges so we fill-in the stroke below.
    // For left vertical scrollbar, we will horizontally flip the canvas in
    // ScrollbarThemeOverlay::paintThumb.
    gfx::Insets fill_insets(kStrokeWidth);
    thumb_rect.Inset(fill_insets + edge_adjust_insets);
  } else {
    ControlColorId color_id = kScrollbarThumb;
    SkAlpha thumb_alpha = SK_AlphaTRANSPARENT;
    switch (state) {
      case NativeTheme::kDisabled:
        break;
      case NativeTheme::kHovered:
        color_id = kScrollbarThumbHovered;
        thumb_alpha = 0x4D;
        break;
      case NativeTheme::kNormal:
        thumb_alpha = 0x33;
        break;
      case NativeTheme::kPressed:
        color_id = kScrollbarThumbPressed;
        thumb_alpha = 0x80;
        break;
      case NativeTheme::kNumStates:
        NOTREACHED();
        break;
    }
    // If there are no scrollbuttons then provide some padding so that the thumb
    // doesn't touch the top of the track.
    const int kThumbPadding = 2;
    const int extra_padding =
        (scrollbar_button_length() == 0) ? kThumbPadding : 0;
    if (part == NativeTheme::kScrollbarVerticalThumb)
      thumb_rect.Inset(kThumbPadding, extra_padding);
    else
      thumb_rect.Inset(extra_padding, kThumbPadding);

    thumb_color =
        (InForcedColorsMode() && features::IsForcedColorsEnabled())
            ? GetControlColor(color_id, color_scheme)
            : SkColorSetA(GetControlColor(kScrollbarThumb, color_scheme),
                          thumb_alpha);
  }

  cc::PaintFlags flags;
  flags.setColor(thumb_color);
  canvas->drawIRect(gfx::RectToSkIRect(thumb_rect), flags);
}

void NativeThemeAura::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect,
                                           ColorScheme color_scheme) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  SkColor bg_color = color_scheme == ui::NativeTheme::ColorScheme::kDark
                         ? SkColorSetRGB(0x12, 0x12, 0x12)
                         : SkColorSetRGB(0xDC, 0xDC, 0xDC);
  flags.setColor(bg_color);
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

gfx::Size NativeThemeAura::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  if (use_overlay_scrollbars_) {
    constexpr int minimum_length =
        kOverlayScrollbarMinimumLength + 2 * kOverlayScrollbarStrokeWidth;

    // Aura overlay scrollbars need a slight tweak from the base sizes.
    switch (part) {
      case kScrollbarHorizontalThumb:
        return gfx::Size(minimum_length, scrollbar_width_);
      case kScrollbarVerticalThumb:
        return gfx::Size(scrollbar_width_, minimum_length);

      default:
        // TODO(bokan): We should probably make sure code using overlay
        // scrollbars isn't asking for part sizes that don't exist.
        // crbug.com/657159.
        break;
    }
  }

  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeAura::DrawPartiallyRoundRect(cc::PaintCanvas* canvas,
                                             const gfx::Rect& rect,
                                             const SkScalar upper_left_radius,
                                             const SkScalar upper_right_radius,
                                             const SkScalar lower_right_radius,
                                             const SkScalar lower_left_radius,
                                             const cc::PaintFlags& flags) {
  gfx::RRectF rounded_rect(
      gfx::RectF(rect), upper_left_radius, upper_left_radius,
      upper_right_radius, upper_right_radius, lower_right_radius,
      lower_right_radius, lower_left_radius, lower_left_radius);

  canvas->drawRRect(static_cast<SkRRect>(rounded_rect), flags);
}

bool NativeThemeAura::SupportsNinePatch(Part part) const {
  if (!IsOverlayScrollbarEnabled())
    return false;

  return part == kScrollbarHorizontalThumb || part == kScrollbarVerticalThumb;
}

gfx::Size NativeThemeAura::GetNinePatchCanvasSize(Part part) const {
  DCHECK(SupportsNinePatch(part));

  return gfx::Size(
      kOverlayScrollbarBorderPatchWidth * 2 + kOverlayScrollbarCenterPatchSize,
      kOverlayScrollbarBorderPatchWidth * 2 + kOverlayScrollbarCenterPatchSize);
}

gfx::Rect NativeThemeAura::GetNinePatchAperture(Part part) const {
  DCHECK(SupportsNinePatch(part));

  return gfx::Rect(
      kOverlayScrollbarBorderPatchWidth, kOverlayScrollbarBorderPatchWidth,
      kOverlayScrollbarCenterPatchSize, kOverlayScrollbarCenterPatchSize);
}

}  // namespace ui
