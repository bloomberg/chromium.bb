// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_theme_linux.h"

#include <limits>

#include "base/logging.h"
#include "grit/gfx_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/gfx_module.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace gfx {

unsigned int NativeThemeLinux::button_length_ = 14;
unsigned int NativeThemeLinux::scrollbar_width_ = 15;
unsigned int NativeThemeLinux::thumb_inactive_color_ = 0xeaeaea;
unsigned int NativeThemeLinux::thumb_active_color_ = 0xf4f4f4;
unsigned int NativeThemeLinux::track_color_ = 0xd3d3d3;

// These are the default dimensions of radio buttons and checkboxes.
static const int kCheckboxAndRadioWidth = 13;
static const int kCheckboxAndRadioHeight = 13;

// These sizes match the sizes in Chromium Win.
static const int kSliderThumbWidth = 11;
static const int kSliderThumbHeight = 21;

static const SkColor kSliderTrackBackgroundColor =
    SkColorSetRGB(0xe3, 0xdd, 0xd8);
static const SkColor kSliderThumbLightGrey = SkColorSetRGB(0xf4, 0xf2, 0xef);
static const SkColor kSliderThumbDarkGrey = SkColorSetRGB(0xea, 0xe5, 0xe0);
static const SkColor kSliderThumbBorderDarkGrey =
    SkColorSetRGB(0x9d, 0x96, 0x8e);

#if !defined(OS_CHROMEOS)
// Chromeos has a different look.
// static
NativeThemeLinux* NativeThemeLinux::instance() {
  // The global NativeThemeLinux instance.
  static NativeThemeLinux s_native_theme;
  return &s_native_theme;
}
#endif

// Get lightness adjusted color.
static SkColor BrightenColor(const color_utils::HSL& hsl, SkAlpha alpha,
    double lightness_amount) {
  color_utils::HSL adjusted = hsl;
  adjusted.l += lightness_amount;
  if (adjusted.l > 1.0)
    adjusted.l = 1.0;
  if (adjusted.l < 0.0)
    adjusted.l = 0.0;

  return color_utils::HSLToSkColor(adjusted, alpha);
}

static SkBitmap* GfxGetBitmapNamed(int key) {
  base::StringPiece data = GfxModule::GetResource(key);
  if (!data.size()) {
    NOTREACHED() << "Unable to load image resource " << key;
    return NULL;
  }

  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(data.data()),
      data.size(), &bitmap)) {
    NOTREACHED() << "Unable to decode image resource " << key;
    return NULL;
  }

  return new SkBitmap(bitmap);
}

NativeThemeLinux::NativeThemeLinux() {
}

NativeThemeLinux::~NativeThemeLinux() {
}

gfx::Size NativeThemeLinux::GetPartSize(Part part) const {
  switch (part) {
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
      return gfx::Size(scrollbar_width_, button_length_);
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      return gfx::Size(button_length_, scrollbar_width_);
    case kScrollbarHorizontalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(2 * scrollbar_width_, scrollbar_width_);
    case kScrollbarVerticalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(scrollbar_width_, 2 * scrollbar_width_);
      break;
    case kScrollbarHorizontalTrack:
      return gfx::Size(0, scrollbar_width_);
    case kScrollbarVerticalTrack:
      return gfx::Size(scrollbar_width_, 0);
    case kCheckbox:
    case kRadio:
      return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
    case kSliderThumb:
      // These sizes match the sizes in Chromium Win.
      return gfx::Size(kSliderThumbWidth, kSliderThumbHeight);
    case kInnerSpinButton:
      return gfx::Size(scrollbar_width_, 0);
    case kPushButton:
    case kTextField:
    case kMenuList:
    case kSliderTrack:
    case kProgressBar:
      return gfx::Size();  // No default size.
  }
  return gfx::Size();
}

void NativeThemeLinux::PaintArrowButton(
    skia::PlatformCanvas* canvas,
    const gfx::Rect& rect, Part direction, State state) {
  int widthMiddle, lengthMiddle;
  SkPaint paint;
  if (direction == kScrollbarUpArrow || direction == kScrollbarDownArrow) {
    widthMiddle = rect.width() / 2 + 1;
    lengthMiddle = rect.height() / 2 + 1;
  } else {
    lengthMiddle = rect.width() / 2 + 1;
    widthMiddle = rect.height() / 2 + 1;
  }

  // Calculate button color.
  SkScalar trackHSV[3];
  SkColorToHSV(track_color_, trackHSV);
  SkColor buttonColor = SaturateAndBrighten(trackHSV, 0, 0.2);
  SkColor backgroundColor = buttonColor;
  if (state == kPressed) {
    SkScalar buttonHSV[3];
    SkColorToHSV(buttonColor, buttonHSV);
    buttonColor = SaturateAndBrighten(buttonHSV, 0, -0.1);
  } else if (state == kHovered) {
    SkScalar buttonHSV[3];
    SkColorToHSV(buttonColor, buttonHSV);
    buttonColor = SaturateAndBrighten(buttonHSV, 0, 0.05);
  }

  SkIRect skrect;
  skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y()
      + rect.height());
  // Paint the background (the area visible behind the rounded corners).
  paint.setColor(backgroundColor);
  canvas->drawIRect(skrect, paint);

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

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(buttonColor);
  canvas->drawPath(outline, paint);

  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  SkScalar thumbHSV[3];
  SkColorToHSV(thumb_inactive_color_, thumbHSV);
  paint.setColor(OutlineColor(trackHSV, thumbHSV));
  canvas->drawPath(outline, paint);

  // If the button is disabled or read-only, the arrow is drawn with the
  // outline color.
  if (state != kDisabled)
    paint.setColor(SK_ColorBLACK);

  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kFill_Style);

  SkPath path;
  // The constants in this block of code are hand-tailored to produce good
  // looking arrows without anti-aliasing.
  switch (direction) {
    case kScrollbarUpArrow:
      path.moveTo(rect.x() + widthMiddle - 4, rect.y() + lengthMiddle + 2);
      path.rLineTo(7, 0);
      path.rLineTo(-4, -4);
      break;
    case kScrollbarDownArrow:
      path.moveTo(rect.x() + widthMiddle - 4, rect.y() + lengthMiddle - 3);
      path.rLineTo(7, 0);
      path.rLineTo(-4, 4);
      break;
    case kScrollbarRightArrow:
      path.moveTo(rect.x() + lengthMiddle - 3, rect.y() + widthMiddle - 4);
      path.rLineTo(0, 7);
      path.rLineTo(4, -4);
      break;
    case kScrollbarLeftArrow:
      path.moveTo(rect.x() + lengthMiddle + 1, rect.y() + widthMiddle - 5);
      path.rLineTo(0, 9);
      path.rLineTo(-4, -4);
      break;
    default:
      break;
  }
  path.close();

  canvas->drawPath(path, paint);
}

void NativeThemeLinux::Paint(skia::PlatformCanvas* canvas,
                             Part part,
                             State state,
                             const gfx::Rect& rect,
                             const ExtraParams& extra) {
  switch (part) {
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      PaintArrowButton(canvas, rect, part, state);
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintScrollbarThumb(canvas, part, state, rect);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintScrollbarTrack(canvas, part, state, extra.scrollbar_track, rect);
      break;
    case kCheckbox:
      PaintCheckbox(canvas, state, rect, extra.button);
      break;
    case kRadio:
      PaintRadio(canvas, state, rect, extra.button);
      break;
    case kPushButton:
      PaintButton(canvas, state, rect, extra.button);
      break;
    case kTextField:
      PaintTextField(canvas, state, rect, extra.text_field);
      break;
    case kMenuList:
      PaintMenuList(canvas, state, rect, extra.menu_list);
      break;
    case kSliderTrack:
      PaintSliderTrack(canvas, state, rect, extra.slider);
      break;
    case kSliderThumb:
      PaintSliderThumb(canvas, state, rect, extra.slider);
      break;
    case kInnerSpinButton:
      PaintInnerSpinButton(canvas, state, rect, extra.inner_spin);
      break;
    case kProgressBar:
      PaintProgressBar(canvas, state, rect, extra.progress_bar);
      break;
  }
}

void NativeThemeLinux::PaintScrollbarTrack(skia::PlatformCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) {
  SkPaint paint;
  SkIRect skrect;

  skrect.set(rect.x(), rect.y(), rect.right(), rect.bottom());
  SkScalar track_hsv[3];
  SkColorToHSV(track_color_, track_hsv);
  paint.setColor(SaturateAndBrighten(track_hsv, 0, 0));
  canvas->drawIRect(skrect, paint);

  SkScalar thumb_hsv[3];
  SkColorToHSV(thumb_inactive_color_, thumb_hsv);

  paint.setColor(OutlineColor(track_hsv, thumb_hsv));
  DrawBox(canvas, rect, paint);
}

void NativeThemeLinux::PaintScrollbarThumb(skia::PlatformCanvas* canvas,
                                           Part part,
                                           State state,
                                           const gfx::Rect& rect) {
  const bool hovered = state == kHovered;
  const int midx = rect.x() + rect.width() / 2;
  const int midy = rect.y() + rect.height() / 2;
  const bool vertical = part == kScrollbarVerticalThumb;

  SkScalar thumb[3];
  SkColorToHSV(hovered ? thumb_active_color_ : thumb_inactive_color_, thumb);

  SkPaint paint;
  paint.setColor(SaturateAndBrighten(thumb, 0, 0.02));

  SkIRect skrect;
  if (vertical)
    skrect.set(rect.x(), rect.y(), midx + 1, rect.y() + rect.height());
  else
    skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), midy + 1);

  canvas->drawIRect(skrect, paint);

  paint.setColor(SaturateAndBrighten(thumb, 0, -0.02));

  if (vertical) {
    skrect.set(
        midx + 1, rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
  } else {
    skrect.set(
        rect.x(), midy + 1, rect.x() + rect.width(), rect.y() + rect.height());
  }

  canvas->drawIRect(skrect, paint);

  SkScalar track[3];
  SkColorToHSV(track_color_, track);
  paint.setColor(OutlineColor(track, thumb));
  DrawBox(canvas, rect, paint);

  if (rect.height() > 10 && rect.width() > 10) {
    const int grippy_half_width = 2;
    const int inter_grippy_offset = 3;
    if (vertical) {
      DrawHorizLine(canvas,
                    midx - grippy_half_width,
                    midx + grippy_half_width,
                    midy - inter_grippy_offset,
                    paint);
      DrawHorizLine(canvas,
                    midx - grippy_half_width,
                    midx + grippy_half_width,
                    midy,
                    paint);
      DrawHorizLine(canvas,
                    midx - grippy_half_width,
                    midx + grippy_half_width,
                    midy + inter_grippy_offset,
                    paint);
    } else {
      DrawVertLine(canvas,
                   midx - inter_grippy_offset,
                   midy - grippy_half_width,
                   midy + grippy_half_width,
                   paint);
      DrawVertLine(canvas,
                   midx,
                   midy - grippy_half_width,
                   midy + grippy_half_width,
                   paint);
      DrawVertLine(canvas,
                   midx + inter_grippy_offset,
                   midy - grippy_half_width,
                   midy + grippy_half_width,
                   paint);
    }
  }
}

void NativeThemeLinux::PaintCheckbox(skia::PlatformCanvas* canvas,
                                     State state,
                                     const gfx::Rect& rect,
                                     const ButtonExtraParams& button) {
  static SkBitmap* image_disabled_indeterminate = GfxGetBitmapNamed(
      IDR_LINUX_CHECKBOX_DISABLED_INDETERMINATE);
  static SkBitmap* image_indeterminate = GfxGetBitmapNamed(
      IDR_LINUX_CHECKBOX_INDETERMINATE);
  static SkBitmap* image_disabled_on = GfxGetBitmapNamed(
      IDR_LINUX_CHECKBOX_DISABLED_ON);
  static SkBitmap* image_on = GfxGetBitmapNamed(IDR_LINUX_CHECKBOX_ON);
  static SkBitmap* image_disabled_off = GfxGetBitmapNamed(
      IDR_LINUX_CHECKBOX_DISABLED_OFF);
  static SkBitmap* image_off = GfxGetBitmapNamed(IDR_LINUX_CHECKBOX_OFF);

  SkBitmap* image = NULL;
  if (button.indeterminate) {
    image = state == kDisabled ? image_disabled_indeterminate
                               : image_indeterminate;
  } else if (button.checked) {
    image = state == kDisabled ? image_disabled_on : image_on;
  } else {
    image = state == kDisabled ? image_disabled_off : image_off;
  }

  gfx::Rect bounds = rect.Center(gfx::Size(image->width(), image->height()));
  DrawBitmapInt(canvas, *image, 0, 0, image->width(), image->height(),
      bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

void NativeThemeLinux::PaintRadio(skia::PlatformCanvas* canvas,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button) {
  static SkBitmap* image_disabled_on = GfxGetBitmapNamed(
      IDR_LINUX_RADIO_DISABLED_ON);
  static SkBitmap* image_on = GfxGetBitmapNamed(IDR_LINUX_RADIO_ON);
  static SkBitmap* image_disabled_off = GfxGetBitmapNamed(
      IDR_LINUX_RADIO_DISABLED_OFF);
  static SkBitmap* image_off = GfxGetBitmapNamed(IDR_LINUX_RADIO_OFF);

  SkBitmap* image = NULL;
  if (state == kDisabled)
    image = button.checked ? image_disabled_on : image_disabled_off;
  else
    image = button.checked ? image_on : image_off;

  gfx::Rect bounds = rect.Center(gfx::Size(image->width(), image->height()));
  DrawBitmapInt(canvas, *image, 0, 0, image->width(), image->height(),
      bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

void NativeThemeLinux::PaintButton(skia::PlatformCanvas* canvas,
                                   State state,
                                   const gfx::Rect& rect,
                                   const ButtonExtraParams& button) {
  SkPaint paint;
  SkRect skrect;
  const int kRight = rect.right();
  const int kBottom = rect.bottom();
  SkColor base_color = button.background_color;

  color_utils::HSL base_hsl;
  color_utils::SkColorToHSL(base_color, &base_hsl);

  // Our standard gradient is from 0xdd to 0xf8. This is the amount of
  // increased luminance between those values.
  SkColor light_color(BrightenColor(base_hsl, SkColorGetA(base_color), 0.105));

  // If the button is too small, fallback to drawing a single, solid color
  if (rect.width() < 5 || rect.height() < 5) {
    paint.setColor(base_color);
    skrect.set(rect.x(), rect.y(), kRight, kBottom);
    canvas->drawRect(skrect, paint);
    return;
  }

  const int kBorderAlpha = state == kHovered ? 0x80 : 0x55;
  paint.setARGB(kBorderAlpha, 0, 0, 0);
  canvas->drawLine(rect.x() + 1, rect.y(), kRight - 1, rect.y(), paint);
  canvas->drawLine(kRight - 1, rect.y() + 1, kRight - 1, kBottom - 1, paint);
  canvas->drawLine(rect.x() + 1, kBottom - 1, kRight - 1, kBottom - 1, paint);
  canvas->drawLine(rect.x(), rect.y() + 1, rect.x(), kBottom - 1, paint);

  paint.setColor(SK_ColorBLACK);
  const int kLightEnd = state == kPressed ? 1 : 0;
  const int kDarkEnd = !kLightEnd;
  SkPoint gradient_bounds[2];
  gradient_bounds[kLightEnd].set(SkIntToScalar(rect.x()),
                                 SkIntToScalar(rect.y()));
  gradient_bounds[kDarkEnd].set(SkIntToScalar(rect.x()),
                                SkIntToScalar(kBottom - 1));
  SkColor colors[2];
  colors[0] = light_color;
  colors[1] = base_color;

  SkShader* shader = SkGradientShader::CreateLinear(
      gradient_bounds, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setShader(shader);
  shader->unref();

  skrect.set(rect.x() + 1, rect.y() + 1, kRight - 1, kBottom - 1);
  canvas->drawRect(skrect, paint);

  paint.setShader(NULL);
  paint.setColor(BrightenColor(base_hsl, SkColorGetA(base_color), -0.0588));
  canvas->drawPoint(rect.x() + 1, rect.y() + 1, paint);
  canvas->drawPoint(kRight - 2, rect.y() + 1, paint);
  canvas->drawPoint(rect.x() + 1, kBottom - 2, paint);
  canvas->drawPoint(kRight - 2, kBottom - 2, paint);
}

void NativeThemeLinux::PaintTextField(skia::PlatformCanvas* canvas,
                                      State state,
                                      const gfx::Rect& rect,
                                      const TextFieldExtraParams& text) {
  // The following drawing code simulates the user-agent css border for
  // text area and text input so that we do not break layout tests. Once we
  // have decided the desired looks, we should update the code here and
  // the layout test expectations.
  SkRect bounds;
  bounds.set(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);

  SkPaint fill_paint;
  fill_paint.setStyle(SkPaint::kFill_Style);
  fill_paint.setColor(text.background_color);
  canvas->drawRect(bounds, fill_paint);

  if (text.is_text_area) {
    // Draw text area border: 1px solid black
    SkPaint stroke_paint;
    fill_paint.setStyle(SkPaint::kStroke_Style);
    fill_paint.setColor(SK_ColorBLACK);
    canvas->drawRect(bounds, fill_paint);
  } else {
    // Draw text input and listbox inset border
    //   Text Input: 2px inset #eee
    //   Listbox: 1px inset #808080
    const SkColor kLightColor = text.is_listbox ?
        SkColorSetRGB(0x80, 0x80, 0x80) : SkColorSetRGB(0xee, 0xee, 0xee);
    const SkColor kDarkColor = text.is_listbox ?
        SkColorSetRGB(0x2c, 0x2c, 0x2c) : SkColorSetRGB(0x9a, 0x9a, 0x9a);
    const int kBorderWidth = text.is_listbox ? 1 : 2;

    SkPaint dark_paint;
    dark_paint.setAntiAlias(true);
    dark_paint.setStyle(SkPaint::kFill_Style);
    dark_paint.setColor(kDarkColor);

    SkPaint light_paint;
    light_paint.setAntiAlias(true);
    light_paint.setStyle(SkPaint::kFill_Style);
    light_paint.setColor(kLightColor);

    int left = rect.x();
    int top = rect.y();
    int right = rect.right();
    int bottom = rect.bottom();

    SkPath path;
    path.incReserve(4);

    // Top
    path.moveTo(SkIntToScalar(left), SkIntToScalar(top));
    path.lineTo(SkIntToScalar(left + kBorderWidth),
                SkIntToScalar(top + kBorderWidth));
    path.lineTo(SkIntToScalar(right - kBorderWidth),
                SkIntToScalar(top + kBorderWidth));
    path.lineTo(SkIntToScalar(right), SkIntToScalar(top));
    canvas->drawPath(path, dark_paint);

    // Bottom
    path.reset();
    path.moveTo(SkIntToScalar(left + kBorderWidth),
                SkIntToScalar(bottom - kBorderWidth));
    path.lineTo(SkIntToScalar(left), SkIntToScalar(bottom));
    path.lineTo(SkIntToScalar(right), SkIntToScalar(bottom));
    path.lineTo(SkIntToScalar(right - kBorderWidth),
                SkIntToScalar(bottom - kBorderWidth));
    canvas->drawPath(path, light_paint);

    // Left
    path.reset();
    path.moveTo(SkIntToScalar(left), SkIntToScalar(top));
    path.lineTo(SkIntToScalar(left), SkIntToScalar(bottom));
    path.lineTo(SkIntToScalar(left + kBorderWidth),
                SkIntToScalar(bottom - kBorderWidth));
    path.lineTo(SkIntToScalar(left + kBorderWidth),
                SkIntToScalar(top + kBorderWidth));
    canvas->drawPath(path, dark_paint);

    // Right
    path.reset();
    path.moveTo(SkIntToScalar(right - kBorderWidth),
                SkIntToScalar(top + kBorderWidth));
    path.lineTo(SkIntToScalar(right - kBorderWidth), SkIntToScalar(bottom));
    path.lineTo(SkIntToScalar(right), SkIntToScalar(bottom));
    path.lineTo(SkIntToScalar(right), SkIntToScalar(top));
    canvas->drawPath(path, light_paint);
  }
}

void NativeThemeLinux::PaintMenuList(skia::PlatformCanvas* canvas,
                                     State state,
                                     const gfx::Rect& rect,
                                     const MenuListExtraParams& menu_list) {
  ButtonExtraParams button = { 0 };
  button.background_color = menu_list.background_color;
  PaintButton(canvas, state, rect, button);

  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);

  SkPath path;
  path.moveTo(menu_list.arrow_x, menu_list.arrow_y - 3);
  path.rLineTo(6, 0);
  path.rLineTo(-3, 6);
  path.close();
  canvas->drawPath(path, paint);
}

void NativeThemeLinux::PaintSliderTrack(skia::PlatformCanvas* canvas,
                                        State state,
                                        const gfx::Rect& rect,
                                        const SliderExtraParams& slider) {
  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  SkPaint paint;
  paint.setColor(kSliderTrackBackgroundColor);

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
  canvas->drawRect(skrect, paint);
}

void NativeThemeLinux::PaintSliderThumb(skia::PlatformCanvas* canvas,
                                        State state,
                                        const gfx::Rect& rect,
                                        const SliderExtraParams& slider) {
  const bool hovered = (state == kHovered) || slider.in_drag;
  const int kMidX = rect.x() + rect.width() / 2;
  const int kMidY = rect.y() + rect.height() / 2;

  SkPaint paint;
  paint.setColor(hovered ? SK_ColorWHITE : kSliderThumbLightGrey);

  SkIRect skrect;
  if (slider.vertical)
    skrect.set(rect.x(), rect.y(), kMidX + 1, rect.bottom());
  else
    skrect.set(rect.x(), rect.y(), rect.right(), kMidY + 1);

  canvas->drawIRect(skrect, paint);

  paint.setColor(hovered ? kSliderThumbLightGrey : kSliderThumbDarkGrey);

  if (slider.vertical)
    skrect.set(kMidX + 1, rect.y(), rect.right(), rect.bottom());
  else
    skrect.set(rect.x(), kMidY + 1, rect.right(), rect.bottom());

  canvas->drawIRect(skrect, paint);

  paint.setColor(kSliderThumbBorderDarkGrey);
  DrawBox(canvas, rect, paint);

  if (rect.height() > 10 && rect.width() > 10) {
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY, paint);
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY - 3, paint);
    DrawHorizLine(canvas, kMidX - 2, kMidX + 2, kMidY + 3, paint);
  }
}

void NativeThemeLinux::PaintInnerSpinButton(skia::PlatformCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const InnerSpinButtonExtraParams& spin_button) {
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

void NativeThemeLinux::PaintProgressBar(skia::PlatformCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const ProgressBarExtraParams& progress_bar) {
  static SkBitmap* bar_image = GfxGetBitmapNamed(IDR_PROGRESS_BAR);
  static SkBitmap* value_image = GfxGetBitmapNamed(IDR_PROGRESS_VALUE);
  static SkBitmap* left_border_image = GfxGetBitmapNamed(
      IDR_PROGRESS_BORDER_LEFT);
  static SkBitmap* right_border_image = GfxGetBitmapNamed(
      IDR_PROGRESS_BORDER_RIGHT);

  double tile_scale = static_cast<double>(rect.height()) /
      bar_image->height();

  int new_tile_width = static_cast<int>(bar_image->width() * tile_scale);
  double tile_scale_x = static_cast<double>(new_tile_width) /
      bar_image->width();

  DrawTiledImage(canvas, *bar_image, 0, 0, tile_scale_x, tile_scale,
      rect.x(), rect.y(), rect.width(), rect.height());

  if (progress_bar.value_rect_width) {

    new_tile_width = static_cast<int>(value_image->width() * tile_scale);
    tile_scale_x = static_cast<double>(new_tile_width) /
        value_image->width();

    DrawTiledImage(canvas, *value_image, 0, 0, tile_scale_x, tile_scale,
        progress_bar.value_rect_x,
        progress_bar.value_rect_y,
        progress_bar.value_rect_width,
        progress_bar.value_rect_height);
  }

  int dest_left_border_width = static_cast<int>(left_border_image->width() *
      tile_scale);
  SkRect dest_rect = {
      SkIntToScalar(rect.x()),
      SkIntToScalar(rect.y()),
      SkIntToScalar(rect.x() + dest_left_border_width),
      SkIntToScalar(rect.bottom())
  };
  canvas->drawBitmapRect(*left_border_image, NULL, dest_rect);

  int dest_right_border_width = static_cast<int>(right_border_image->width() *
      tile_scale);
  dest_rect.set(SkIntToScalar(rect.right() - dest_right_border_width),
      SkIntToScalar(rect.y()),
      SkIntToScalar(rect.right()),
      SkIntToScalar(rect.bottom()));
  canvas->drawBitmapRect(*right_border_image, NULL, dest_rect);
}

bool NativeThemeLinux::IntersectsClipRectInt(
    skia::PlatformCanvas* canvas, int x, int y, int w, int h) {
  SkRect clip;
  return canvas->getClipBounds(&clip) &&
      clip.intersect(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + w),
                     SkIntToScalar(y + h));
}

void NativeThemeLinux::DrawVertLine(SkCanvas* canvas,
                                    int x,
                                    int y1,
                                    int y2,
                                    const SkPaint& paint) const {
  SkIRect skrect;
  skrect.set(x, y1, x + 1, y2 + 1);
  canvas->drawIRect(skrect, paint);
}

void NativeThemeLinux::DrawHorizLine(SkCanvas* canvas,
                                     int x1,
                                     int x2,
                                     int y,
                                     const SkPaint& paint) const {
  SkIRect skrect;
  skrect.set(x1, y, x2 + 1, y + 1);
  canvas->drawIRect(skrect, paint);
}

void NativeThemeLinux::DrawBox(SkCanvas* canvas,
                               const gfx::Rect& rect,
                               const SkPaint& paint) const {
  const int right = rect.x() + rect.width() - 1;
  const int bottom = rect.y() + rect.height() - 1;
  DrawHorizLine(canvas, rect.x(), right, rect.y(), paint);
  DrawVertLine(canvas, right, rect.y(), bottom, paint);
  DrawHorizLine(canvas, rect.x(), right, bottom, paint);
  DrawVertLine(canvas, rect.x(), rect.y(), bottom, paint);
}

void NativeThemeLinux::DrawBitmapInt(
    skia::PlatformCanvas* canvas, const SkBitmap& bitmap,
    int src_x, int src_y, int src_w, int src_h,
    int dest_x, int dest_y, int dest_w, int dest_h) {
  DLOG_ASSERT(src_x + src_w < std::numeric_limits<int16_t>::max() &&
              src_y + src_h < std::numeric_limits<int16_t>::max());
  if (src_w <= 0 || src_h <= 0 || dest_w <= 0 || dest_h <= 0) {
    NOTREACHED() << "Attempting to draw bitmap to/from an empty rect!";
    return;
  }

  if (!IntersectsClipRectInt(canvas, dest_x, dest_y, dest_w, dest_h))
    return;

  SkRect dest_rect = { SkIntToScalar(dest_x),
                       SkIntToScalar(dest_y),
                       SkIntToScalar(dest_x + dest_w),
                       SkIntToScalar(dest_y + dest_h) };

  if (src_w == dest_w && src_h == dest_h) {
    // Workaround for apparent bug in Skia that causes image to occasionally
    // shift.
    SkIRect src_rect = { src_x, src_y, src_x + src_w, src_y + src_h };
    canvas->drawBitmapRect(bitmap, &src_rect, dest_rect);
    return;
  }

  // Make a bitmap shader that contains the bitmap we want to draw. This is
  // basically what SkCanvas.drawBitmap does internally, but it gives us
  // more control over quality and will use the mipmap in the source image if
  // it has one, whereas drawBitmap won't.
  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  SkMatrix shader_scale;
  shader_scale.setScale(SkFloatToScalar(static_cast<float>(dest_w) / src_w),
                        SkFloatToScalar(static_cast<float>(dest_h) / src_h));
  shader_scale.preTranslate(SkIntToScalar(-src_x), SkIntToScalar(-src_y));
  shader_scale.postTranslate(SkIntToScalar(dest_x), SkIntToScalar(dest_y));
  shader->setLocalMatrix(shader_scale);

  // The rect will be filled by the bitmap.
  SkPaint p;
  p.setFilterBitmap(true);
  p.setShader(shader);
  shader->unref();
  canvas->drawRect(dest_rect, p);
}

void NativeThemeLinux::DrawTiledImage(SkCanvas* canvas,
   const SkBitmap& bitmap,
   int src_x, int src_y, double tile_scale_x, double tile_scale_y,
   int dest_x, int dest_y, int w, int h) const {
  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  if (tile_scale_x != 1.0 || tile_scale_y != 1.0) {
    SkMatrix shader_scale;
    shader_scale.setScale(SkDoubleToScalar(tile_scale_x),
                          SkDoubleToScalar(tile_scale_y));
    shader->setLocalMatrix(shader_scale);
  }

  SkPaint paint;
  paint.setShader(shader);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  canvas->save();
  canvas->translate(SkIntToScalar(dest_x - src_x),
                    SkIntToScalar(dest_y - src_y));
  canvas->clipRect(SkRect::MakeXYWH(src_x, src_y, w, h));
  canvas->drawPaint(paint);
  canvas->restore();
}

SkScalar NativeThemeLinux::Clamp(SkScalar value,
                                 SkScalar min,
                                 SkScalar max) const {
  return std::min(std::max(value, min), max);
}

SkColor NativeThemeLinux::SaturateAndBrighten(SkScalar* hsv,
                                              SkScalar saturate_amount,
                                              SkScalar brighten_amount) const {
  SkScalar color[3];
  color[0] = hsv[0];
  color[1] = Clamp(hsv[1] + saturate_amount, 0.0, 1.0);
  color[2] = Clamp(hsv[2] + brighten_amount, 0.0, 1.0);
  return SkHSVToColor(color);
}

SkColor NativeThemeLinux::OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const {
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
  SkScalar min_diff = Clamp((hsv1[1] + hsv2[1]) * 1.2, 0.28, 0.5);
  SkScalar diff = Clamp(fabs(hsv1[2] - hsv2[2]) / 2, min_diff, 0.5);

  if (hsv1[2] + hsv2[2] > 1.0)
    diff = -diff;

  return SaturateAndBrighten(hsv2, -0.2, diff);
}

void NativeThemeLinux::SetScrollbarColors(unsigned inactive_color,
                                          unsigned active_color,
                                          unsigned track_color) const {
  thumb_inactive_color_ = inactive_color;
  thumb_active_color_ = active_color;
  track_color_ = track_color;
}

}  // namespace gfx
