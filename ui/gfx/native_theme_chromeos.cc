// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_theme_chromeos.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "grit/gfx_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// Color constants. See theme_draw for details.

// Border color used for many widgets.
const SkColor kBaseStroke = SkColorSetRGB(0x8F, 0x8F, 0x8F);

// Disabled border color used for many widgets.
const SkColor kDisabledBaseStroke = SkColorSetRGB(0xB7, 0xB7, 0xB7);

// Common gradient stop and colors.
const SkColor kBaseGradient0 = SkColorSetRGB(255, 255, 255);
const SkColor kBaseGradient1 = SkColorSetRGB(255, 255, 255);
const SkColor kBaseGradient2 = SkColorSetRGB(0xD8, 0xD8, 0xD8);

const SkColor kPressedGradient0 = SkColorSetRGB(0x95, 0x95, 0x95);
const SkColor kPressedGradient1 = SkColorSetRGB(0xE3, 0xE3, 0xE3);

const SkColor kIndicatorStrokeDisabledColor = SkColorSetRGB(0xB4, 0xB4, 0xB4);
// TODO: these are wrong, what should they be?
const SkColor kIndicatorStrokePressedColor = SkColorSetRGB(0, 0, 0);
const SkColor kIndicatorStrokeColor = SkColorSetRGB(0, 0, 0);

const SkColor kRadioIndicatorGradient0 = SkColorSetRGB(0, 0, 0);
const SkColor kRadioIndicatorGradient1 = SkColorSetRGB(0x83, 0x83, 0x83);

const SkColor kRadioIndicatorDisabledGradient0 =
    SkColorSetRGB(0xB4, 0xB4, 0xB4);
const SkColor kRadioIndicatorDisabledGradient1 =
    SkColorSetRGB(0xB7, 0xB7, 0xB7);

// Progressbar colors
const SkColor kProgressBarBackgroundGradient0 = SkColorSetRGB(0xBB, 0xBB, 0xBB);
const SkColor kProgressBarBackgroundGradient1 = SkColorSetRGB(0xE7, 0xE7, 0xE7);
const SkColor kProgressBarBackgroundGradient2 = SkColorSetRGB(0xFE, 0xFE, 0xFE);
const SkColor kProgressBarBorderStroke = SkColorSetRGB(0xA1, 0xA1, 0xA1);

const SkColor kProgressBarIndicatorGradient0 = SkColorSetRGB(100, 116, 147);
const SkColor kProgressBarIndicatorGradient1 = SkColorSetRGB(65, 73, 87);

const SkColor kProgressBarIndicatorDisabledGradient0 =
    SkColorSetRGB(229, 232, 237);
const SkColor kProgressBarIndicatorDisabledGradient1 =
    SkColorSetRGB(224, 225, 227);

const SkColor kProgressBarIndicatorStroke = SkColorSetRGB(0x4A, 0x4A, 0x4A);
const SkColor kProgressBarIndicatorDisabledStroke =
    SkColorSetARGB(0x80, 0x4A, 0x4A, 0x4A);

const SkColor kProgressBarIndicatorInnerStroke =
    SkColorSetARGB(0x3F, 0xFF, 0xFF, 0xFF);  // 0.25 white
const SkColor kProgressBarIndicatorInnerShadow =
    SkColorSetARGB(0x54, 0xFF, 0xFF, 0xFF);  // 0.33 white

// Geometry constants

const int kBorderCornerRadius = 3;

const int kRadioIndicatorSize = 7;

const int kSliderThumbWidth = 16;
const int kSliderThumbHeight = 16;
const int kSliderTrackSize = 6;

void GetRoundRectPathWithPadding(const gfx::Rect rect,
                                 int corner_radius,
                                 SkScalar padding,
                                 SkPath* path) {
  SkRect bounds = { SkDoubleToScalar(rect.x()) + padding,
                    SkDoubleToScalar(rect.y()) + padding,
                    SkDoubleToScalar(rect.right()) - padding,
                    SkDoubleToScalar(rect.bottom()) - padding };
  path->addRoundRect(bounds,
                     SkIntToScalar(corner_radius) - padding,
                     SkIntToScalar(corner_radius) - padding);
}

void GetRoundRectPath(const gfx::Rect rect,
                      int corner_radius,
                      SkPath* path) {
  // Add 0.5 pixel padding so that antialias paint does not touch extra pixels.
  GetRoundRectPathWithPadding(rect, corner_radius, SkIntToScalar(1) / 2,
                              path);
}

void GetGradientPaintForRect(const gfx::Rect& rect,
                             const SkColor* colors,
                             const SkScalar* stops,
                             int count,
                             SkPaint* paint) {
  paint->setStyle(SkPaint::kFill_Style);
  paint->setAntiAlias(true);

  SkPoint points[2];
  points[0].set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()));
  points[1].set(SkIntToScalar(rect.x()), SkIntToScalar(rect.bottom()));

  SkShader* shader = SkGradientShader::CreateLinear(points,
      colors, stops, count, SkShader::kClamp_TileMode);

  paint->setShader(shader);
  // Unref shader after paint takes ownership, otherwise never deleted.
  shader->unref();
}

void GetGradientPaintForRect(const gfx::Rect& rect,
                             SkColor start_color,
                             SkColor end_color,
                             SkPaint* paint) {
  SkColor colors[2] = { start_color, end_color };
  GetGradientPaintForRect(rect, colors, NULL, 2, paint);
}

void GetButtonGradientPaint(const gfx::Rect bounds,
                            gfx::NativeThemeBase::State state,
                            SkPaint* paint) {
  if (state == gfx::NativeThemeBase::kPressed) {
    static const SkColor kGradientColors[2] = {
        kPressedGradient0,
        kPressedGradient1
    };

    static const SkScalar kGradientPoints[2] = {
        SkIntToScalar(0),
        SkIntToScalar(1)
    };

    GetGradientPaintForRect(bounds,
        kGradientColors, kGradientPoints, arraysize(kGradientPoints),
        paint);
  } else {
    static const SkColor kGradientColors[3] = {
        kBaseGradient0,
        kBaseGradient1,
        kBaseGradient2
    };

    static const SkScalar kGradientPoints[3] = {
        SkIntToScalar(0),
        SkDoubleToScalar(0.5),
        SkIntToScalar(1)
    };

    GetGradientPaintForRect(bounds,
        kGradientColors, kGradientPoints, arraysize(kGradientPoints),
        paint);
  }
}

void GetStrokePaint(SkColor color, SkPaint* paint) {
  paint->setStyle(SkPaint::kStroke_Style);
  paint->setAntiAlias(true);
  paint->setColor(color);
}

void GetStrokePaint(gfx::NativeThemeBase::State state, SkPaint* paint) {

  if (state == gfx::NativeThemeBase::kDisabled)
    GetStrokePaint(kDisabledBaseStroke, paint);
  else
    GetStrokePaint(kBaseStroke, paint);
}

void GetIndicatorStrokePaint(gfx::NativeThemeBase::State state,
                             SkPaint* paint) {
  paint->setStyle(SkPaint::kStroke_Style);
  paint->setAntiAlias(true);

  if (state == gfx::NativeThemeBase::kDisabled)
    paint->setColor(kIndicatorStrokeDisabledColor);
  else if (state == gfx::NativeThemeBase::kPressed)
    paint->setColor(kIndicatorStrokePressedColor);
  else
    paint->setColor(kIndicatorStrokeColor);
}

void GetRadioIndicatorGradientPaint(const gfx::Rect bounds,
                                    gfx::NativeThemeBase::State state,
                                    SkPaint* paint) {
  paint->setStyle(SkPaint::kFill_Style);
  paint->setAntiAlias(true);

  SkPoint points[2];
  points[0].set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()));
  points[1].set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.bottom()));

  static const SkScalar kGradientPoints[2] = {
      SkIntToScalar(0),
      SkIntToScalar(1)
  };

  if (state == gfx::NativeThemeBase::kDisabled) {
    static const SkColor kGradientColors[2] = {
        kRadioIndicatorDisabledGradient0,
        kRadioIndicatorDisabledGradient1
    };

    GetGradientPaintForRect(bounds,
        kGradientColors, kGradientPoints, arraysize(kGradientPoints),
        paint);
  } else {
    static const SkColor kGradientColors[2] = {
        kRadioIndicatorGradient0,
        kRadioIndicatorGradient1
    };

    GetGradientPaintForRect(bounds,
        kGradientColors, kGradientPoints, arraysize(kGradientPoints),
        paint);
  }
}

}  // namespace

namespace gfx {

// static
const NativeTheme* NativeTheme::instance() {
  return NativeThemeChromeos::instance();
}

// static
const NativeThemeChromeos* NativeThemeChromeos::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeChromeos, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeChromeos::NativeThemeChromeos() {
}

NativeThemeChromeos::~NativeThemeChromeos() {
}

gfx::Size NativeThemeChromeos::GetPartSize(Part part,
                                           State state,
                                           const ExtraParams& extra) const {
  // This function might be called from Worker process during html layout
  // without calling GfxModule::SetResourceProvider. So using dimension
  // constants instead of getting it from resource images.
  static const int kScrollbarWidth = 13;
  static const int kScrollbarArrowUpHeight = 12;
  static const int kScrollbarArrowDownHeight = 12;

  int width = 0, height = 0;
  switch (part) {
    case kScrollbarUpArrow:
      width = kScrollbarWidth;
      height = kScrollbarArrowUpHeight;
      break;
    case kScrollbarDownArrow:
      width = kScrollbarWidth;
      height = kScrollbarArrowDownHeight;
      break;
    case kScrollbarLeftArrow:
      width = kScrollbarArrowUpHeight;
      height = kScrollbarWidth;
      break;
    case kScrollbarRightArrow:
      width = kScrollbarArrowDownHeight;
      height = kScrollbarWidth;
      break;
    case kScrollbarHorizontalTrack:
      width = 0;
      height = kScrollbarWidth;
      break;
    case kScrollbarVerticalTrack:
      width = kScrollbarWidth;
      height = 0;
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      // allow thumb to be square but no shorter.
      width = kScrollbarWidth;
      height = kScrollbarWidth;
      break;
    case kSliderThumb:
      width = kSliderThumbWidth;
      height = kSliderThumbHeight;
      break;
    case kInnerSpinButton:
      return gfx::Size(kScrollbarWidth, 0);
    default:
      return NativeThemeBase::GetPartSize(part, state, extra);
  }
  return gfx::Size(width, height);
}

void NativeThemeChromeos::PaintScrollbarTrack(
    SkCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (part == kScrollbarVerticalTrack) {
    SkBitmap* background = rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND);
    SkBitmap* border_up = rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_UP);
    SkBitmap* border_down =
        rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_DOWN);
    // Draw track background.
    DrawBitmapInt(
        canvas, *background,
        0, 0, background->width(), 1,
        rect.x(), rect.y(), rect.width(), rect.height());
    // Draw up button lower border.
    canvas->drawBitmap(*border_up, extra_params.track_x, extra_params.track_y);
    // Draw down button upper border.
    canvas->drawBitmap(
        *border_down,
        extra_params.track_x,
        extra_params.track_y + extra_params.track_height - border_down->height()
        );
  } else {
    SkBitmap* background =
        GetHorizontalBitmapNamed(IDR_SCROLL_BACKGROUND);
    SkBitmap* border_left =
        GetHorizontalBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_UP);
    SkBitmap* border_right =
        GetHorizontalBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_DOWN);
    // Draw track background.
    DrawBitmapInt(
        canvas, *background,
        0, 0, 1, background->height(),
        rect.x(), rect.y(), rect.width(), rect.height());
    // Draw left button right border.
    canvas->drawBitmap(*border_left,extra_params.track_x, extra_params.track_y);
    // Draw right button left border.
    canvas->drawBitmap(
        *border_right,
        extra_params.track_x + extra_params.track_width - border_right->width(),
        extra_params.track_y);
  }
}

void NativeThemeChromeos::PaintArrowButton(SkCanvas* canvas,
    const gfx::Rect& rect, Part part, State state) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int resource_id =
      (part == kScrollbarUpArrow || part == kScrollbarLeftArrow) ?
          IDR_SCROLL_ARROW_UP : IDR_SCROLL_ARROW_DOWN;
  if (state == kHovered)
    resource_id++;
  else if (state == kPressed)
    resource_id += 2;
  SkBitmap* bitmap;
  if (part == kScrollbarUpArrow || part == kScrollbarDownArrow)
    bitmap = rb.GetBitmapNamed(resource_id);
  else
    bitmap = GetHorizontalBitmapNamed(resource_id);
  DrawBitmapInt(canvas, *bitmap,
      0, 0, bitmap->width(), bitmap->height(),
      rect.x(), rect.y(), rect.width(), rect.height());
}

void NativeThemeChromeos::PaintScrollbarThumb(SkCanvas* canvas,
    Part part, State state, const gfx::Rect& rect) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int resource_id = IDR_SCROLL_THUMB;
  if (state == kHovered)
    resource_id++;
  else if (state == kPressed)
    resource_id += 2;
  if (part == kScrollbarVerticalThumb) {
    SkBitmap* bitmap = rb.GetBitmapNamed(resource_id);
    // Top
    DrawBitmapInt(
        canvas, *bitmap,
        0, 1, bitmap->width(), 5,
        rect.x(), rect.y(), rect.width(), 5);
    // Middle
    DrawBitmapInt(
        canvas, *bitmap,
        0, 7, bitmap->width(), 1,
        rect.x(), rect.y() + 5, rect.width(), rect.height() - 10);
    // Bottom
    DrawBitmapInt(
        canvas, *bitmap,
        0, 8, bitmap->width(), 5,
        rect.x(), rect.y() + rect.height() - 5, rect.width(), 5);
  } else {
    SkBitmap* bitmap = GetHorizontalBitmapNamed(resource_id);
    // Left
    DrawBitmapInt(
        canvas, *bitmap,
        1, 0, 5, bitmap->height(),
        rect.x(), rect.y(), 5, rect.height());
    // Middle
    DrawBitmapInt(
        canvas, *bitmap,
        7, 0, 1, bitmap->height(),
        rect.x() + 5, rect.y(), rect.width() - 10, rect.height());
    // Right
    DrawBitmapInt(
        canvas, *bitmap,
        8, 0, 5, bitmap->height(),
        rect.x() + rect.width() - 5, rect.y(), 5, rect.height());
  }
}

void NativeThemeChromeos::PaintCheckbox(SkCanvas* canvas,
    State state, const gfx::Rect& rect,
    const ButtonExtraParams& button) const {
  PaintButtonLike(canvas, state, rect, true);

  if (button.checked) {
    SkPaint indicator_paint;
    GetIndicatorStrokePaint(state, &indicator_paint);
    indicator_paint.setStrokeWidth(2);

    const int kMidX = rect.x() + rect.width() / 2;
    const int kMidY = rect.y() + rect.height() / 2;
    canvas->drawLine(SkIntToScalar(rect.x() + 3), SkIntToScalar(kMidY),
        SkIntToScalar(kMidX - 1), SkIntToScalar(rect.bottom() - 3),
        indicator_paint);
    canvas->drawLine(SkIntToScalar(kMidX - 1), SkIntToScalar(rect.bottom() - 3),
        SkIntToScalar(rect.right() - 3), SkIntToScalar(rect.y() + 3),
        indicator_paint);
  }
}

void NativeThemeChromeos::PaintRadio(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& button) const {
  gfx::Point center = rect.CenterPoint();
  SkPath border;
  border.addCircle(SkIntToScalar(center.x()), SkIntToScalar(center.y()),
                   SkDoubleToScalar(rect.width() / 2.0));

  SkPaint fill_paint;
  GetButtonGradientPaint(rect, state, &fill_paint);
  canvas->drawPath(border, fill_paint);

  SkPaint stroke_paint;
  GetStrokePaint(state, &stroke_paint);
  canvas->drawPath(border, stroke_paint);

  if (button.checked) {
    SkPath indicator_border;
    indicator_border.addCircle(SkIntToScalar(center.x()),
                               SkIntToScalar(center.y()),
                               SkDoubleToScalar(kRadioIndicatorSize / 2.0));

    SkPaint indicator_fill_paint;
    GetRadioIndicatorGradientPaint(rect, state, &indicator_fill_paint);
    canvas->drawPath(indicator_border, indicator_fill_paint);

    SkPaint indicator_paint;
    GetIndicatorStrokePaint(state, &indicator_paint);
    canvas->drawPath(indicator_border, indicator_paint);
  }
}

void NativeThemeChromeos::PaintButton(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ButtonExtraParams& button) const {
  PaintButtonLike(canvas, state, rect, button.has_border);
}

void NativeThemeChromeos::PaintTextField(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const TextFieldExtraParams& text) const {
  if (rect.height() == 0)
    return;

  SkColor background_color = text.background_color;

  SkPaint fill_paint;
  fill_paint.setStyle(SkPaint::kFill_Style);
  if (state == kDisabled) {
    fill_paint.setColor(background_color);
  } else {
    SkScalar base_hsv[3];
    SkColorToHSV(background_color, base_hsv);

    const SkColor gradient_colors[3] = {
        SaturateAndBrighten(base_hsv, 0, -0.18),
        background_color,
        background_color
    };

    const SkScalar gradient_points[3] = {
        SkIntToScalar(0),
        SkDoubleToScalar(4.0 / rect.height()),
        SkIntToScalar(1)
    };

    SkPoint points[2];
    points[0].set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()));
    points[1].set(SkIntToScalar(rect.x()), SkIntToScalar(rect.bottom()));

    GetGradientPaintForRect(rect,
        gradient_colors, gradient_points, arraysize(gradient_points),
        &fill_paint);
  }

  SkPath border;
  GetRoundRectPath(rect, kBorderCornerRadius, &border);
  canvas->drawPath(border, fill_paint);

  SkPaint stroke_paint;
  GetStrokePaint(state, &stroke_paint);
  canvas->drawPath(border, stroke_paint);
}

void NativeThemeChromeos::PaintSliderTrack(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const SliderExtraParams& slider) const {
  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  gfx::Rect track_bounds;
  if (slider.vertical) {
    track_bounds.SetRect(std::max(rect.x(), kMidX - kSliderTrackSize / 2),
                         rect.y(),
                         std::min(rect.width(), kSliderTrackSize),
                         rect.height());
  } else {
    track_bounds.SetRect(rect.x(),
                         std::max(rect.y(), kMidY - kSliderTrackSize / 2),
                         rect.width(),
                         std::min(rect.height(), kSliderTrackSize));
  }

  SkPath border;
  GetRoundRectPath(track_bounds, kBorderCornerRadius, &border);

  SkPaint fill_paint;
  // Use normal button background.
  GetButtonGradientPaint(rect, kNormal, &fill_paint);
  canvas->drawPath(border, fill_paint);

  SkPaint stroke_paint;
  GetStrokePaint(state, &stroke_paint);
  canvas->drawPath(border, stroke_paint);
}

void NativeThemeChromeos::PaintSliderThumb(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const SliderExtraParams& slider) const {
  if (state != kDisabled && slider.in_drag)
    state = kPressed;

  PaintButtonLike(canvas, state, rect, true);
}

void NativeThemeChromeos::PaintInnerSpinButton(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const InnerSpinButtonExtraParams& spin_button) const {
  // Adjust bounds to compensate the overridden "2px inset" parent border.
  gfx::Rect bounds = rect;
  bounds.Inset(0, -1, -1, -1);

  NativeThemeBase::PaintInnerSpinButton(canvas, state, bounds, spin_button);
}

void NativeThemeChromeos::PaintMenuPopupBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  static const SkColor kGradientColors[2] = {
      SK_ColorWHITE,
      SkColorSetRGB(0xF0, 0xF0, 0xF0)
  };

  static const SkScalar kGradientPoints[2] = {
      SkIntToScalar(0),
      SkIntToScalar(1)
  };

  SkPoint points[2];
  points[0].set(SkIntToScalar(0), SkIntToScalar(0));
  points[1].set(SkIntToScalar(0), SkIntToScalar(rect.height()));

  SkShader* shader = SkGradientShader::CreateLinear(points,
      kGradientColors, kGradientPoints, arraysize(kGradientPoints),
      SkShader::kRepeat_TileMode);
  DCHECK(shader);

  SkPaint paint;
  paint.setShader(shader);
  shader->unref();

  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkRect sk_rect;
  sk_rect.set(SkIntToScalar(0), SkIntToScalar(0),
              SkIntToScalar(rect.width()), SkIntToScalar(rect.height()));
  canvas->drawRect(sk_rect, paint);
}

void NativeThemeChromeos::PaintProgressBar(SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& progress_bar) const {
  static const int kBorderWidth = 1;
  static const SkColor kBackgroundColors[] = {
    kProgressBarBackgroundGradient0,
    kProgressBarBackgroundGradient1,
    kProgressBarBackgroundGradient2
  };

  static const SkScalar kBackgroundPoints[] = {
    SkDoubleToScalar(0),
    SkDoubleToScalar(0.1),
    SkDoubleToScalar(1)
  };
  static const SkColor kBackgroundBorderColor = kProgressBarBorderStroke;

  // Draw background.
  SkPath border;
  GetRoundRectPath(rect, kBorderCornerRadius, &border);

  SkPaint fill_paint;
  GetGradientPaintForRect(rect,
      kBackgroundColors, kBackgroundPoints, arraysize(kBackgroundPoints),
      &fill_paint);
  canvas->drawPath(border, fill_paint);

  SkPaint stroke_paint;
  GetStrokePaint(kBackgroundBorderColor, &stroke_paint);
  canvas->drawPath(border, stroke_paint);

  if (progress_bar.value_rect_width > 1) {
    bool enabled = state != kDisabled;
    gfx::Rect value_rect(progress_bar.value_rect_x,
                         progress_bar.value_rect_y,
                         progress_bar.value_rect_width,
                         progress_bar.value_rect_height);

    const SkColor bar_color_start = enabled ?
        kProgressBarIndicatorGradient0 :
        kProgressBarIndicatorDisabledGradient0;
    const SkColor bar_color_end = enabled ?
        kProgressBarIndicatorGradient1 :
        kProgressBarIndicatorDisabledGradient1;

    const SkColor bar_outer_color = enabled ?
        kProgressBarIndicatorStroke :
        kProgressBarIndicatorDisabledStroke;

    const SkColor bar_inner_border_color = kProgressBarIndicatorInnerStroke;
    const SkColor bar_inner_shadow_color = kProgressBarIndicatorInnerShadow;

    // Draw bar background
    SkPath value_border;
    GetRoundRectPath(value_rect, kBorderCornerRadius, &value_border);

    SkPaint value_fill_paint;
    GetGradientPaintForRect(rect,bar_color_start, bar_color_end,
        &value_fill_paint);
    canvas->drawPath(value_border, value_fill_paint);

    // Draw inner stroke and shadow if wide enough.
    if (progress_bar.value_rect_width > 2 * kBorderWidth) {
      canvas->save();

      SkPath inner_path;
      GetRoundRectPathWithPadding(value_rect, kBorderCornerRadius,
          SkIntToScalar(kBorderWidth), &inner_path);
      canvas->clipPath(inner_path);

      // Draw bar inner stroke
      gfx::Rect inner_stroke_rect = value_rect;
      inner_stroke_rect.Inset(kBorderWidth, kBorderWidth);

      SkPath inner_stroke_path;
      GetRoundRectPath(inner_stroke_rect, kBorderCornerRadius - kBorderWidth,
          &inner_stroke_path);

      SkPaint inner_stroke_paint;
      GetStrokePaint(bar_inner_border_color, &inner_stroke_paint);

      canvas->drawPath(inner_stroke_path, inner_stroke_paint);

      // Draw bar inner shadow
      gfx::Rect inner_shadow_rect(progress_bar.value_rect_x,
          progress_bar.value_rect_y + kBorderWidth,
          progress_bar.value_rect_width,
          progress_bar.value_rect_height);
      SkPath inner_shadow_path;
      GetRoundRectPath(inner_shadow_rect, kBorderCornerRadius,
          &inner_shadow_path);

      SkPaint inner_shadow_paint;
      GetStrokePaint(bar_inner_shadow_color, &inner_shadow_paint);

      canvas->drawPath(inner_shadow_path, inner_shadow_paint);

      canvas->restore();
    }

    // Draw bar stroke
    SkPaint value_stroke_paint;
    GetStrokePaint(bar_outer_color, &value_stroke_paint);
    canvas->drawPath(value_border, value_stroke_paint);
  }
}

SkBitmap* NativeThemeChromeos::GetHorizontalBitmapNamed(int resource_id) const {
  SkImageMap::const_iterator found = horizontal_bitmaps_.find(resource_id);
  if (found != horizontal_bitmaps_.end())
    return found->second;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* vertical_bitmap = rb.GetBitmapNamed(resource_id);

  if (vertical_bitmap) {
    SkBitmap transposed_bitmap =
        SkBitmapOperations::CreateTransposedBtmap(*vertical_bitmap);
    SkBitmap* horizontal_bitmap = new SkBitmap(transposed_bitmap);

    horizontal_bitmaps_[resource_id] = horizontal_bitmap;
    return horizontal_bitmap;
  }
  return NULL;
}

void NativeThemeChromeos::PaintButtonLike(SkCanvas* canvas,
    State state, const gfx::Rect& rect, bool stroke_border) const {
  SkPath border;
  GetRoundRectPath(rect, kBorderCornerRadius, &border);

  SkPaint fill_paint;
  GetButtonGradientPaint(rect, state, &fill_paint);
  canvas->drawPath(border, fill_paint);

  if (stroke_border) {
    SkPaint stroke_paint;
    GetStrokePaint(state, &stroke_paint);
    canvas->drawPath(border, stroke_paint);
  }
}

}  // namespace gfx

