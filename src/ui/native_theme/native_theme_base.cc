// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

namespace {

// These are the default dimensions of radio buttons and checkboxes.
const int kCheckboxAndRadioWidth = 13;
const int kCheckboxAndRadioHeight = 13;

// These sizes match the sizes in Chromium Win.
const int kSliderThumbWidth = 11;
const int kSliderThumbHeight = 21;

const SkColor kSliderTrackBackgroundColor =
    SkColorSetRGB(0xe3, 0xdd, 0xd8);
const SkColor kSliderThumbLightGrey = SkColorSetRGB(0xf4, 0xf2, 0xef);
const SkColor kSliderThumbDarkGrey = SkColorSetRGB(0xea, 0xe5, 0xe0);
const SkColor kSliderThumbBorderDarkGrey =
    SkColorSetRGB(0x9d, 0x96, 0x8e);

const SkColor kTextBorderColor = SkColorSetRGB(0xa9, 0xa9, 0xa9);

const SkColor kProgressBorderColor = kTextBorderColor;
const SkColor kProgressTickColor = SkColorSetRGB(0xED, 0xED, 0xED);
const SkColor kProgressValueColor = gfx::kGoogleBlue300;

const SkColor kMenuPopupBackgroundColor = SkColorSetRGB(210, 225, 246);

const int kDefaultScrollbarWidth = 15;
const int kDefaultScrollbarButtonLength = 14;

const SkColor kCheckboxTinyColor = SK_ColorGRAY;
const SkColor kCheckboxShadowColor = SkColorSetARGB(0x15, 0, 0, 0);
const SkColor kCheckboxShadowHoveredColor = SkColorSetARGB(0x1F, 0, 0, 0);
const SkColor kCheckboxShadowDisabledColor = SkColorSetARGB(0, 0, 0, 0);
const SkColor kCheckboxGradientColors[] = {
    SkColorSetRGB(0xed, 0xed, 0xed),
    SkColorSetRGB(0xde, 0xde, 0xde) };
const SkColor kCheckboxGradientPressedColors[] = {
    SkColorSetRGB(0xe7, 0xe7, 0xe7),
    SkColorSetRGB(0xd7, 0xd7, 0xd7) };
const SkColor kCheckboxGradientHoveredColors[] = {
    SkColorSetRGB(0xf0, 0xf0, 0xf0),
    SkColorSetRGB(0xe0, 0xe0, 0xe0) };
const SkColor kCheckboxGradientDisabledColors[] = {
    SkColorSetARGB(0x80, 0xed, 0xed, 0xed),
    SkColorSetARGB(0x80, 0xde, 0xde, 0xde) };
const SkColor kCheckboxBorderColor = SkColorSetARGB(0x40, 0, 0, 0);
const SkColor kCheckboxBorderHoveredColor = SkColorSetARGB(0x4D, 0, 0, 0);
const SkColor kCheckboxBorderDisabledColor = SkColorSetARGB(0x20, 0, 0, 0);
const SkColor kCheckboxStrokeColor = SkColorSetARGB(0xB3, 0, 0, 0);
const SkColor kCheckboxStrokeDisabledColor = SkColorSetARGB(0x59, 0, 0, 0);
const SkColor kRadioDotColor = SkColorSetRGB(0x66, 0x66, 0x66);
const SkColor kRadioDotDisabledColor = SkColorSetARGB(0x80, 0x66, 0x66, 0x66);

// Get lightness adjusted color.
SkColor BrightenColor(const color_utils::HSL& hsl, SkAlpha alpha,
    double lightness_amount) {
  color_utils::HSL adjusted = hsl;
  adjusted.l += lightness_amount;
  if (adjusted.l > 1.0)
    adjusted.l = 1.0;
  if (adjusted.l < 0.0)
    adjusted.l = 0.0;

  return color_utils::HSLToSkColor(adjusted, alpha);
}

}  // namespace

namespace ui {

gfx::Size NativeThemeBase::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  switch (part) {
    // Please keep these in the order of NativeTheme::Part.
    case kCheckbox:
      return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
    case kInnerSpinButton:
      return gfx::Size(scrollbar_width_, 0);
    case kMenuList:
      return gfx::Size();  // No default size.
    case kMenuPopupBackground:
      return gfx::Size();  // No default size.
    case kMenuItemBackground:
    case kProgressBar:
    case kPushButton:
      return gfx::Size();  // No default size.
    case kRadio:
      return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
      return gfx::Size(scrollbar_width_, scrollbar_button_length_);
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      return gfx::Size(scrollbar_button_length_, scrollbar_width_);
    case kScrollbarHorizontalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(2 * scrollbar_width_, scrollbar_width_);
    case kScrollbarVerticalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(scrollbar_width_, 2 * scrollbar_width_);
    case kScrollbarHorizontalTrack:
      return gfx::Size(0, scrollbar_width_);
    case kScrollbarVerticalTrack:
      return gfx::Size(scrollbar_width_, 0);
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      NOTIMPLEMENTED();
      break;
    case kSliderTrack:
      return gfx::Size();  // No default size.
    case kSliderThumb:
      // These sizes match the sizes in Chromium Win.
      return gfx::Size(kSliderThumbWidth, kSliderThumbHeight);
    case kTabPanelBackground:
      NOTIMPLEMENTED();
      break;
    case kTextField:
      return gfx::Size();  // No default size.
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kWindowResizeGripper:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED() << "Unknown theme part: " << part;
      break;
  }
  return gfx::Size();
}

void NativeThemeBase::Paint(cc::PaintCanvas* canvas,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra) const {
  if (rect.IsEmpty())
    return;

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(rect));

  switch (part) {
    // Please keep these in the order of NativeTheme::Part.
    case kCheckbox:
      PaintCheckbox(canvas, state, rect, extra.button);
      break;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    case kFrameTopArea:
      PaintFrameTopArea(canvas, state, rect, extra.frame_top_area);
      break;
#endif
    case kInnerSpinButton:
      PaintInnerSpinButton(canvas, state, rect, extra.inner_spin);
      break;
    case kMenuList:
      PaintMenuList(canvas, state, rect, extra.menu_list);
      break;
    case kMenuPopupBackground:
      PaintMenuPopupBackground(canvas, rect.size(), extra.menu_background);
      break;
    case kMenuPopupSeparator:
      PaintMenuSeparator(canvas, state, rect, extra.menu_separator);
      break;
    case kMenuItemBackground:
      PaintMenuItemBackground(canvas, state, rect, extra.menu_item);
      break;
    case kProgressBar:
      PaintProgressBar(canvas, state, rect, extra.progress_bar);
      break;
    case kPushButton:
      PaintButton(canvas, state, rect, extra.button);
      break;
    case kRadio:
      PaintRadio(canvas, state, rect, extra.button);
      break;
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      if (scrollbar_button_length_ > 0)
        PaintArrowButton(canvas, rect, part, state);
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintScrollbarThumb(canvas, part, state, rect,
                          extra.scrollbar_thumb.scrollbar_theme);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrack(canvas, part, state, extra.scrollbar_track, rect);
      break;
    case kScrollbarHorizontalGripper:
    case kScrollbarVerticalGripper:
      // Invoked by views scrollbar code, don't care about for non-win
      // implementations, so no NOTIMPLEMENTED.
      break;
    case kScrollbarCorner:
      PaintScrollbarCorner(canvas, state, rect);
      break;
    case kSliderTrack:
      PaintSliderTrack(canvas, state, rect, extra.slider);
      break;
    case kSliderThumb:
      PaintSliderThumb(canvas, state, rect, extra.slider);
      break;
    case kTabPanelBackground:
      NOTIMPLEMENTED();
      break;
    case kTextField:
      PaintTextField(canvas, state, rect, extra.text_field);
      break;
    case kTrackbarThumb:
    case kTrackbarTrack:
    case kWindowResizeGripper:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED() << "Unknown theme part: " << part;
      break;
  }

  canvas->restore();
}

bool NativeThemeBase::SupportsNinePatch(Part part) const {
  return false;
}

gfx::Size NativeThemeBase::GetNinePatchCanvasSize(Part part) const {
  NOTREACHED() << "NativeThemeBase doesn't support nine-patch resources.";
  return gfx::Size();
}

gfx::Rect NativeThemeBase::GetNinePatchAperture(Part part) const {
  NOTREACHED() << "NativeThemeBase doesn't support nine-patch resources.";
  return gfx::Rect();
}

bool NativeThemeBase::UsesHighContrastColors() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceHighContrast);
}

NativeThemeBase::NativeThemeBase()
    : scrollbar_width_(kDefaultScrollbarWidth),
      scrollbar_button_length_(kDefaultScrollbarButtonLength) {
}

NativeThemeBase::~NativeThemeBase() {
}

void NativeThemeBase::PaintArrowButton(cc::PaintCanvas* canvas,
                                       const gfx::Rect& rect,
                                       Part direction,
                                       State state) const {
  cc::PaintFlags flags;

  // Calculate button color.
  SkScalar trackHSV[3];
  SkColorToHSV(track_color_, trackHSV);
  SkColor buttonColor = SaturateAndBrighten(trackHSV, 0, 0.2f);
  SkColor backgroundColor = buttonColor;
  if (state == kPressed) {
    SkScalar buttonHSV[3];
    SkColorToHSV(buttonColor, buttonHSV);
    buttonColor = SaturateAndBrighten(buttonHSV, 0, -0.1f);
  } else if (state == kHovered) {
    SkScalar buttonHSV[3];
    SkColorToHSV(buttonColor, buttonHSV);
    buttonColor = SaturateAndBrighten(buttonHSV, 0, 0.05f);
  }

  SkIRect skrect;
  skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y()
      + rect.height());
  // Paint the background (the area visible behind the rounded corners).
  flags.setColor(backgroundColor);
  canvas->drawIRect(skrect, flags);

  // Paint the button's outline and fill the middle
  SkPath outline;
  switch (direction) {
    case kScrollbarUpArrow:
      outline.moveTo(rect.x() + 0.5, rect.y() + rect.height() + 0.5);
      outline.rLineTo(0, -(rect.height() - 2));
      outline.rLineTo(2, -2);
      outline.rLineTo(rect.width() - 5, 0);
      outline.rLineTo(2, 2);
      outline.rLineTo(0, rect.height() - 2);
      break;
    case kScrollbarDownArrow:
      outline.moveTo(rect.x() + 0.5, rect.y() - 0.5);
      outline.rLineTo(0, rect.height() - 2);
      outline.rLineTo(2, 2);
      outline.rLineTo(rect.width() - 5, 0);
      outline.rLineTo(2, -2);
      outline.rLineTo(0, -(rect.height() - 2));
      break;
    case kScrollbarRightArrow:
      outline.moveTo(rect.x() - 0.5, rect.y() + 0.5);
      outline.rLineTo(rect.width() - 2, 0);
      outline.rLineTo(2, 2);
      outline.rLineTo(0, rect.height() - 5);
      outline.rLineTo(-2, 2);
      outline.rLineTo(-(rect.width() - 2), 0);
      break;
    case kScrollbarLeftArrow:
      outline.moveTo(rect.x() + rect.width() + 0.5, rect.y() + 0.5);
      outline.rLineTo(-(rect.width() - 2), 0);
      outline.rLineTo(-2, 2);
      outline.rLineTo(0, rect.height() - 5);
      outline.rLineTo(2, 2);
      outline.rLineTo(rect.width() - 2, 0);
      break;
    default:
      break;
  }
  outline.close();

  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(buttonColor);
  canvas->drawPath(outline, flags);

  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  SkScalar thumbHSV[3];
  SkColorToHSV(thumb_inactive_color_, thumbHSV);
  flags.setColor(OutlineColor(trackHSV, thumbHSV));
  canvas->drawPath(outline, flags);

  PaintArrow(canvas, rect, direction, GetArrowColor(state));
}

void NativeThemeBase::PaintArrow(cc::PaintCanvas* gc,
                                 const gfx::Rect& rect,
                                 Part direction,
                                 SkColor color) const {
  cc::PaintFlags flags;
  flags.setColor(color);

  SkPath path = PathForArrow(rect, direction);

  gc->drawPath(path, flags);
}

SkPath NativeThemeBase::PathForArrow(const gfx::Rect& rect,
                                     Part direction) const {
  gfx::Rect bounding_rect = BoundingRectForArrow(rect);
  const gfx::PointF center = gfx::RectF(bounding_rect).CenterPoint();
  SkPath path;
  SkMatrix transform;
  transform.setIdentity();
  if (direction == kScrollbarUpArrow || direction == kScrollbarDownArrow) {
    int arrow_altitude = bounding_rect.height() / 2 + 1;
    path.moveTo(bounding_rect.x(), bounding_rect.bottom());
    path.rLineTo(bounding_rect.width(), 0);
    path.rLineTo(-bounding_rect.width() / 2.0f, -arrow_altitude);
    path.close();
    path.offset(0, -arrow_altitude / 2 + 1);
    if (direction == kScrollbarDownArrow) {
      transform.setScale(1, -1, center.x(), center.y());
    }
  } else {
    int arrow_altitude = bounding_rect.width() / 2 + 1;
    path.moveTo(bounding_rect.x(), bounding_rect.y());
    path.rLineTo(0, bounding_rect.height());
    path.rLineTo(arrow_altitude, -bounding_rect.height() / 2.0f);
    path.close();
    path.offset(arrow_altitude / 2, 0);
    if (direction == kScrollbarLeftArrow) {
      transform.setScale(-1, 1, center.x(), center.y());
    }
  }
  path.transform(transform);

  return path;
}

gfx::Rect NativeThemeBase::BoundingRectForArrow(const gfx::Rect& rect) const {
  const std::pair<int, int> rect_sides =
      std::minmax(rect.width(), rect.height());
  const int side_length_inset = 2 * std::ceil(rect_sides.second / 4.f);
  const int side_length =
      std::min(rect_sides.first, rect_sides.second - side_length_inset);
  // When there are an odd number of pixels, put the extra on the top/left.
  return gfx::Rect(rect.x() + (rect.width() - side_length + 1) / 2,
                   rect.y() + (rect.height() - side_length + 1) / 2,
                   side_length, side_length);
}

void NativeThemeBase::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  cc::PaintFlags flags;
  SkIRect skrect;

  skrect.set(rect.x(), rect.y(), rect.right(), rect.bottom());
  SkScalar track_hsv[3];
  SkColorToHSV(track_color_, track_hsv);
  flags.setColor(SaturateAndBrighten(track_hsv, 0, 0));
  canvas->drawIRect(skrect, flags);

  SkScalar thumb_hsv[3];
  SkColorToHSV(thumb_inactive_color_, thumb_hsv);

  flags.setColor(OutlineColor(track_hsv, thumb_hsv));
  DrawBox(canvas, rect, flags);
}

void NativeThemeBase::PaintScrollbarThumb(cc::PaintCanvas* canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect,
                                          ScrollbarOverlayColorTheme) const {
  const bool hovered = state == kHovered;
  const int midx = rect.x() + rect.width() / 2;
  const int midy = rect.y() + rect.height() / 2;
  const bool vertical = part == kScrollbarVerticalThumb;

  SkScalar thumb[3];
  SkColorToHSV(hovered ? thumb_active_color_ : thumb_inactive_color_, thumb);

  cc::PaintFlags flags;
  flags.setColor(SaturateAndBrighten(thumb, 0, 0.02f));

  SkIRect skrect;
  if (vertical)
    skrect.set(rect.x(), rect.y(), midx + 1, rect.y() + rect.height());
  else
    skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), midy + 1);

  canvas->drawIRect(skrect, flags);

  flags.setColor(SaturateAndBrighten(thumb, 0, -0.02f));

  if (vertical) {
    skrect.set(
        midx + 1, rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
  } else {
    skrect.set(
        rect.x(), midy + 1, rect.x() + rect.width(), rect.y() + rect.height());
  }

  canvas->drawIRect(skrect, flags);

  SkScalar track[3];
  SkColorToHSV(track_color_, track);
  flags.setColor(OutlineColor(track, thumb));
  DrawBox(canvas, rect, flags);

  if (rect.height() > 10 && rect.width() > 10) {
    const int grippy_half_width = 2;
    const int inter_grippy_offset = 3;
    if (vertical) {
      DrawHorizLine(canvas, midx - grippy_half_width, midx + grippy_half_width,
                    midy - inter_grippy_offset, flags);
      DrawHorizLine(canvas, midx - grippy_half_width, midx + grippy_half_width,
                    midy, flags);
      DrawHorizLine(canvas, midx - grippy_half_width, midx + grippy_half_width,
                    midy + inter_grippy_offset, flags);
    } else {
      DrawVertLine(canvas, midx - inter_grippy_offset, midy - grippy_half_width,
                   midy + grippy_half_width, flags);
      DrawVertLine(canvas, midx, midy - grippy_half_width,
                   midy + grippy_half_width, flags);
      DrawVertLine(canvas, midx + inter_grippy_offset, midy - grippy_half_width,
                   midy + grippy_half_width, flags);
    }
  }
}

void NativeThemeBase::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect) const {}

void NativeThemeBase::PaintCheckbox(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    const ButtonExtraParams& button) const {
  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect,
                                           SkIntToScalar(2));
  if (!skrect.isEmpty()) {
    // Draw the checkmark / dash.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    if (state == kDisabled)
      flags.setColor(kCheckboxStrokeDisabledColor);
    else
      flags.setColor(kCheckboxStrokeColor);
    if (button.indeterminate) {
      SkPath dash;
      dash.moveTo(skrect.x() + skrect.width() * 0.16,
                  (skrect.y() + skrect.bottom()) / 2);
      dash.rLineTo(skrect.width() * 0.68, 0);
      flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.2));
      canvas->drawPath(dash, flags);
    } else if (button.checked) {
      SkPath check;
      check.moveTo(skrect.x() + skrect.width() * 0.2,
                   skrect.y() + skrect.height() * 0.5);
      check.rLineTo(skrect.width() * 0.2, skrect.height() * 0.2);
      flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.23));
      check.lineTo(skrect.right() - skrect.width() * 0.2,
                   skrect.y() + skrect.height() * 0.2);
      canvas->drawPath(check, flags);
    }
  }
}

// Draws the common elements of checkboxes and radio buttons.
// Returns the rectangle within which any additional decorations should be
// drawn, or empty if none.
SkRect NativeThemeBase::PaintCheckboxRadioCommon(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const SkScalar borderRadius) const {
  SkRect skrect = gfx::RectToSkRect(rect);

  // Use the largest square that fits inside the provided rectangle.
  // No other browser seems to support non-square widget, so accidentally
  // having non-square sizes is common (eg. amazon and webkit dev tools).
  if (skrect.width() != skrect.height()) {
    SkScalar size = SkMinScalar(skrect.width(), skrect.height());
    skrect.inset((skrect.width() - size) / 2, (skrect.height() - size) / 2);
  }

  // If the rectangle is too small then paint only a rectangle.  We don't want
  // to have to worry about '- 1' and '+ 1' calculations below having overflow
  // or underflow.
  if (skrect.width() <= 2) {
    cc::PaintFlags flags;
    flags.setColor(kCheckboxTinyColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRect(skrect, flags);
    // Too small to draw anything more.
    return SkRect::MakeEmpty();
  }

  // Make room for padding/drop shadow.
  AdjustCheckboxRadioRectForPadding(&skrect);

  // Draw the drop shadow below the widget.
  if (state != kPressed) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    SkRect shadowRect = skrect;
    shadowRect.offset(0, 1);
    if (state == kDisabled)
      flags.setColor(kCheckboxShadowDisabledColor);
    else if (state == kHovered)
      flags.setColor(kCheckboxShadowHoveredColor);
    else
      flags.setColor(kCheckboxShadowColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRoundRect(shadowRect, borderRadius, borderRadius, flags);
  }

  // Draw the gradient-filled rectangle
  SkPoint gradient_bounds[3];
  gradient_bounds[0].set(skrect.x(), skrect.y());
  gradient_bounds[1].set(skrect.x(), skrect.y() + skrect.height() * 0.38);
  gradient_bounds[2].set(skrect.x(), skrect.bottom());
  const SkColor* startEndColors;
  if (state == kPressed)
    startEndColors = kCheckboxGradientPressedColors;
  else if (state == kHovered)
    startEndColors = kCheckboxGradientHoveredColors;
  else if (state == kDisabled)
    startEndColors = kCheckboxGradientDisabledColors;
  else /* kNormal */
    startEndColors = kCheckboxGradientColors;
  SkColor colors[3] = {startEndColors[0], startEndColors[0], startEndColors[1]};
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds, colors, nullptr, 3, SkShader::kClamp_TileMode));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(skrect, borderRadius, borderRadius, flags);
  flags.setShader(nullptr);

  // Draw the border.
  if (state == kHovered)
    flags.setColor(kCheckboxBorderHoveredColor);
  else if (state == kDisabled)
    flags.setColor(kCheckboxBorderDisabledColor);
  else
    flags.setColor(kCheckboxBorderColor);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(SkIntToScalar(1));
  skrect.inset(SkFloatToScalar(.5f), SkFloatToScalar(.5f));
  canvas->drawRoundRect(skrect, borderRadius, borderRadius, flags);

  // Return the rectangle excluding the drop shadow for drawing any additional
  // decorations.
  return skrect;
}

void NativeThemeBase::PaintRadio(cc::PaintCanvas* canvas,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ButtonExtraParams& button) const {
  // Most of a radio button is the same as a checkbox, except the the rounded
  // square is a circle (i.e. border radius >= 100%).
  const SkScalar radius = SkFloatToScalar(
      static_cast<float>(std::max(rect.width(), rect.height())) / 2);
  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, radius);
  if (!skrect.isEmpty() && button.checked) {
    // Draw the dot.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    if (state == kDisabled)
      flags.setColor(kRadioDotDisabledColor);
    else
      flags.setColor(kRadioDotColor);
    skrect.inset(skrect.width() * 0.25, skrect.height() * 0.25);
    // Use drawRoundedRect instead of drawOval to be completely consistent
    // with the border in PaintCheckboxRadioNewCommon.
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
}

void NativeThemeBase::PaintButton(cc::PaintCanvas* canvas,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button) const {
  cc::PaintFlags flags;
  SkRect skrect = gfx::RectToSkRect(rect);
  SkColor base_color = button.background_color;

  color_utils::HSL base_hsl;
  color_utils::SkColorToHSL(base_color, &base_hsl);

  // Our standard gradient is from 0xdd to 0xf8. This is the amount of
  // increased luminance between those values.
  SkColor light_color(BrightenColor(base_hsl, SkColorGetA(base_color), 0.105));

  // If the button is too small, fallback to drawing a single, solid color
  if (rect.width() < 5 || rect.height() < 5) {
    flags.setColor(base_color);
    canvas->drawRect(skrect, flags);
    return;
  }

  flags.setColor(SK_ColorBLACK);
  SkPoint gradient_bounds[2] = {
    gfx::PointToSkPoint(rect.origin()),
    gfx::PointToSkPoint(rect.bottom_left() - gfx::Vector2d(0, 1))
  };
  if (state == kPressed)
    std::swap(gradient_bounds[0], gradient_bounds[1]);
  SkColor colors[2] = { light_color, base_color };

  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds, colors, nullptr, 2, SkShader::kClamp_TileMode));

  canvas->drawRoundRect(skrect, SkIntToScalar(1), SkIntToScalar(1), flags);
  flags.setShader(nullptr);

  if (button.has_border) {
    int border_alpha = state == kHovered ? 0x80 : 0x55;
    if (button.is_focused) {
      border_alpha = 0xff;
      flags.setColor(GetSystemColor(kColorId_FocusedBorderColor));
    }
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(1));
    flags.setAlpha(border_alpha);
    skrect.inset(SkFloatToScalar(.5f), SkFloatToScalar(.5f));
    canvas->drawRoundRect(skrect, SkIntToScalar(1), SkIntToScalar(1), flags);
  }
}

void NativeThemeBase::PaintTextField(cc::PaintCanvas* canvas,
                                     State state,
                                     const gfx::Rect& rect,
                                     const TextFieldExtraParams& text) const {
  SkRect bounds;
  bounds.set(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);

  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(text.background_color);
  canvas->drawRect(bounds, fill_flags);

  // Text INPUT, listbox SELECT, and TEXTAREA have consistent borders.
  // border: 1px solid #a9a9a9
  cc::PaintFlags stroke_flags;
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setColor(kTextBorderColor);
  canvas->drawRect(bounds, stroke_flags);
}

void NativeThemeBase::PaintMenuList(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  // If a border radius is specified, we let the WebCore paint the background
  // and the border of the control.
  if (!menu_list.has_border_radius) {
    ButtonExtraParams button = { 0 };
    button.background_color = menu_list.background_color;
    button.has_border = menu_list.has_border;
    PaintButton(canvas, state, rect, button);
  }

  cc::PaintFlags flags;
  flags.setColor(menu_list.arrow_color);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  int arrow_size = menu_list.arrow_size;
  gfx::Rect arrow(
    menu_list.arrow_x,
    menu_list.arrow_y - (arrow_size / 2),
    arrow_size,
    arrow_size);

  // Constrain to the paint rect.
  arrow.Intersect(rect);

  SkPath path;
  path.moveTo(arrow.x(), arrow.y());
  path.lineTo(arrow.right(), arrow.y());
  path.lineTo(arrow.x() + arrow.width() / 2, arrow.bottom());
  path.close();
  canvas->drawPath(path, flags);
}

void NativeThemeBase::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  canvas->drawColor(kMenuPopupBackgroundColor, SkBlendMode::kSrc);
}

void NativeThemeBase::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  // By default don't draw anything over the normal background.
}

void NativeThemeBase::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& menu_separator) const {
  cc::PaintFlags flags;
  flags.setColor(GetSystemColor(ui::NativeTheme::kColorId_MenuSeparatorColor));
  canvas->drawRect(gfx::RectToSkRect(*menu_separator.paint_rect), flags);
}

void NativeThemeBase::PaintSliderTrack(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const SliderExtraParams& slider) const {
  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  cc::PaintFlags flags;
  flags.setColor(kSliderTrackBackgroundColor);

  SkRect skrect;
  if (slider.vertical) {
    skrect.set(std::max(rect.x(), kMidX - 2),
               rect.y(),
               std::min(rect.right(), kMidX + 2),
               rect.bottom());
  } else {
    skrect.set(rect.x(),
               std::max(rect.y(), kMidY - 2),
               rect.right(),
               std::min(rect.bottom(), kMidY + 2));
  }
  canvas->drawRect(skrect, flags);
}

void NativeThemeBase::PaintSliderThumb(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const SliderExtraParams& slider) const {
  const bool hovered = (state == kHovered) || slider.in_drag;
  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  cc::PaintFlags flags;
  flags.setColor(hovered ? SK_ColorWHITE : kSliderThumbLightGrey);

  SkIRect skrect;
  if (slider.vertical)
    skrect.set(rect.x(), rect.y(), kMidX + 1, rect.bottom());
  else
    skrect.set(rect.x(), rect.y(), rect.right(), kMidY + 1);

  canvas->drawIRect(skrect, flags);

  flags.setColor(hovered ? kSliderThumbLightGrey : kSliderThumbDarkGrey);

  if (slider.vertical)
    skrect.set(kMidX + 1, rect.y(), rect.right(), rect.bottom());
  else
    skrect.set(rect.x(), kMidY + 1, rect.right(), rect.bottom());

  canvas->drawIRect(skrect, flags);

  flags.setColor(kSliderThumbBorderDarkGrey);
  DrawBox(canvas, rect, flags);

  if (rect.height() > 10 && rect.width() > 10) {
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY, flags);
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY - 3, flags);
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY + 3, flags);
  }
}

void NativeThemeBase::PaintInnerSpinButton(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const InnerSpinButtonExtraParams& spin_button) const {
  if (spin_button.read_only)
    state = kDisabled;

  State north_state = state;
  State south_state = state;
  if (spin_button.spin_up)
    south_state = south_state != kDisabled ? kNormal : kDisabled;
  else
    north_state = north_state != kDisabled ? kNormal : kDisabled;

  gfx::Rect half = rect;
  half.set_height(rect.height() / 2);
  PaintArrowButton(canvas, half, kScrollbarUpArrow, north_state);

  half.set_y(rect.y() + rect.height() / 2);
  PaintArrowButton(canvas, half, kScrollbarDownArrow, south_state);
}

void NativeThemeBase::PaintProgressBar(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& progress_bar) const {
  DCHECK(!rect.IsEmpty());

  canvas->drawColor(SK_ColorWHITE);

  // Draw the tick marks. The spacing between the tick marks is adjusted to
  // evenly divide into the width.
  SkPath path;
  int stroke_width = std::max(1, rect.height() / 18);
  int tick_width = 16 * stroke_width;
  int ticks = rect.width() / tick_width + (rect.width() % tick_width ? 1 : 0);
  SkScalar tick_spacing = SkIntToScalar(rect.width()) / ticks;
  for (int i = 1; i < ticks; ++i) {
    path.moveTo(rect.x() + i * tick_spacing, rect.y());
    path.rLineTo(0, rect.height());
  }
  cc::PaintFlags stroke_flags;
  stroke_flags.setColor(kProgressTickColor);
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setStrokeWidth(stroke_width);
  canvas->drawPath(path, stroke_flags);

  // Draw progress.
  gfx::Rect progress_rect(progress_bar.value_rect_x, progress_bar.value_rect_y,
                          progress_bar.value_rect_width,
                          progress_bar.value_rect_height);
  cc::PaintFlags progress_flags;
  progress_flags.setColor(kProgressValueColor);
  progress_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRect(gfx::RectToSkRect(progress_rect), progress_flags);

  // Draw the border.
  gfx::RectF border_rect(rect);
  border_rect.Inset(stroke_width / 2.0f, stroke_width / 2.0f);
  stroke_flags.setColor(kProgressBorderColor);
  canvas->drawRect(gfx::RectFToSkRect(border_rect), stroke_flags);
}

void NativeThemeBase::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& frame_top_area) const {
  cc::PaintFlags flags;
  flags.setColor(frame_top_area.default_background_color);
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeBase::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // By default we only take 1px from right and bottom for the drop shadow.
  rect->iset(rect->x(), rect->y(), rect->right() - 1, rect->bottom() - 1);
}

SkColor NativeThemeBase::SaturateAndBrighten(SkScalar* hsv,
                                             SkScalar saturate_amount,
                                             SkScalar brighten_amount) const {
  SkScalar color[3];
  color[0] = hsv[0];
  color[1] = Clamp(hsv[1] + saturate_amount, 0.0, 1.0);
  color[2] = Clamp(hsv[2] + brighten_amount, 0.0, 1.0);
  return SkHSVToColor(color);
}

SkColor NativeThemeBase::GetArrowColor(State state) const {
  if (state != kDisabled)
    return SK_ColorBLACK;

  SkScalar track_hsv[3];
  SkColorToHSV(track_color_, track_hsv);
  SkScalar thumb_hsv[3];
  SkColorToHSV(thumb_inactive_color_, thumb_hsv);
  return OutlineColor(track_hsv, thumb_hsv);
}

void NativeThemeBase::DrawVertLine(cc::PaintCanvas* canvas,
                                   int x,
                                   int y1,
                                   int y2,
                                   const cc::PaintFlags& flags) const {
  SkIRect skrect;
  skrect.set(x, y1, x + 1, y2 + 1);
  canvas->drawIRect(skrect, flags);
}

void NativeThemeBase::DrawHorizLine(cc::PaintCanvas* canvas,
                                    int x1,
                                    int x2,
                                    int y,
                                    const cc::PaintFlags& flags) const {
  SkIRect skrect;
  skrect.set(x1, y, x2 + 1, y + 1);
  canvas->drawIRect(skrect, flags);
}

void NativeThemeBase::DrawBox(cc::PaintCanvas* canvas,
                              const gfx::Rect& rect,
                              const cc::PaintFlags& flags) const {
  const int right = rect.x() + rect.width() - 1;
  const int bottom = rect.y() + rect.height() - 1;
  DrawHorizLine(canvas, rect.x(), right, rect.y(), flags);
  DrawVertLine(canvas, right, rect.y(), bottom, flags);
  DrawHorizLine(canvas, rect.x(), right, bottom, flags);
  DrawVertLine(canvas, rect.x(), rect.y(), bottom, flags);
}

SkScalar NativeThemeBase::Clamp(SkScalar value,
                                SkScalar min,
                                SkScalar max) const {
  return std::min(std::max(value, min), max);
}

SkColor NativeThemeBase::OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const {
  // GTK Theme engines have way too much control over the layout of
  // the scrollbar. We might be able to more closely approximate its
  // look-and-feel, if we sent whole images instead of just colors
  // from the browser to the renderer. But even then, some themes
  // would just break.
  //
  // So, instead, we don't even try to 100% replicate the look of
  // the native scrollbar. We render our own version, but we make
  // sure to pick colors that blend in nicely with the system GTK
  // theme. In most cases, we can just sample a couple of pixels
  // from the system scrollbar and use those colors to draw our
  // scrollbar.
  //
  // This works fine for the track color and the overall thumb
  // color. But it fails spectacularly for the outline color used
  // around the thumb piece.  Not all themes have a clearly defined
  // outline. For some of them it is partially transparent, and for
  // others the thickness is very unpredictable.
  //
  // So, instead of trying to approximate the system theme, we
  // instead try to compute a reasonable looking choice based on the
  // known color of the track and the thumb piece. This is difficult
  // when trying to deal both with high- and low-contrast themes,
  // and both with positive and inverted themes.
  //
  // The following code has been tested to look OK with all of the
  // default GTK themes.
  SkScalar min_diff = Clamp((hsv1[1] + hsv2[1]) * 1.2f, 0.28f, 0.5f);
  SkScalar diff = Clamp(fabs(hsv1[2] - hsv2[2]) / 2, min_diff, 0.5f);

  if (hsv1[2] + hsv2[2] > 1.0)
    diff = -diff;

  return SaturateAndBrighten(hsv2, -0.2f, diff);
}

}  // namespace ui
