// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

namespace {

const SkColor kScrollerTrackGradientColors[] = {
    SkColorSetRGB(0xEF, 0xEF, 0xEF),
    SkColorSetRGB(0xF9, 0xF9, 0xF9),
    SkColorSetRGB(0xFD, 0xFD, 0xFD),
    SkColorSetRGB(0xF6, 0xF6, 0xF6) };
const SkColor kScrollerTrackInnerBorderColor = SkColorSetRGB(0xE4, 0xE4, 0xE4);
const SkColor kScrollerTrackOuterBorderColor = SkColorSetRGB(0xEF, 0xEF, 0xEF);
const SkColor kScrollerThumbColor = SkColorSetARGB(0x38, 0, 0, 0);
const SkColor kScrollerThumbHoverColor = SkColorSetARGB(0x80, 0, 0, 0);
const int kScrollerTrackBorderWidth = 1;

// The amount the thumb is inset from both the ends and the sides of the track.
const int kScrollerThumbInset = 3;

// Values calculated by reading pixels and solving simultaneous equations
// derived from "A over B" alpha compositing. Steps: Sample the semi-transparent
// pixel over two backgrounds; P1, P2 over backgrounds B1, B2. Use the color
// value between 0.0 and 1.0 (i.e. divide by 255.0). Then,
// alpha = (P2 - P1 + B1 - B2) / (B1 - B2)
// color = (P1 - B1 + alpha * B1) / alpha.
const SkColor kMenuPopupBackgroundColor = SkColorSetARGB(255, 255, 255, 255);
const SkColor kMenuSeparatorColor = SkColorSetARGB(243, 228, 228, 228);
const SkColor kMenuBorderColor = SkColorSetARGB(60, 0, 0, 0);

const SkColor kMenuPopupBackgroundColorYosemite =
    SkColorSetARGB(255, 230, 230, 230);

// Hardcoded color used for some existing dialogs in Chrome's Cocoa UI.
const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);

// Color for the highlighted text in a control when that control doesn't have
// keyboard focus.
const SkColor kUnfocusedSelectedTextBackgroundColor =
    SkColorSetRGB(220, 220, 220);

// On 10.6 and 10.7 there is no way to get components from system colors. Here,
// system colors are just opaque objects that can paint themselves and otherwise
// tell you nothing. In 10.8, some of the system color classes have incomplete
// implementations and throw exceptions even attempting to convert using
// -[NSColor colorUsingColorSpace:], so don't bother there either.
// This function paints a single pixel to a 1x1 swatch and reads it back.
SkColor GetSystemColorUsingSwatch(NSColor* color) {
  SkColor swatch;
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  const size_t bytes_per_row = 4;
  static_assert(sizeof(swatch) == bytes_per_row, "skcolor should be 4 bytes");
  CGBitmapInfo bitmap_info =
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
  base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      &swatch, 1, 1, 8, bytes_per_row, color_space, bitmap_info));

  NSGraphicsContext* drawing_context =
      [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:drawing_context];
  [color drawSwatchInRect:NSMakeRect(0, 0, 1, 1)];
  [NSGraphicsContext restoreGraphicsState];
  return swatch;
}

// NSColor has a number of methods that return system colors (i.e. controlled by
// user preferences). This function converts the color given by an NSColor class
// method to an SkColor. Official documentation suggests developers only rely on
// +[NSColor selectedTextBackgroundColor] and +[NSColor selectedControlColor],
// but other colors give a good baseline. For many, a gradient is involved; the
// palette chosen based on the enum value given by +[NSColor currentColorTint].
// Apple's documentation also suggests to use NSColorList, but the system color
// list is just populated with class methods on NSColor.
SkColor NSSystemColorToSkColor(NSColor* color) {
  if (base::mac::IsOSMountainLionOrEarlier())
    return GetSystemColorUsingSwatch(color);

  // System colors use the an NSNamedColorSpace called "System", so first step
  // is to convert the color into something that can be worked with.
  NSColor* device_color =
      [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
  if (device_color)
    return skia::NSDeviceColorToSkColor(device_color);

  // Sometimes the conversion is not possible, but we can get an approximation
  // by going through a CGColorRef. Note that simply using NSColor methods for
  // accessing components for system colors results in exceptions like
  // "-numberOfComponents not valid for the NSColor NSNamedColorSpace System
  // windowBackgroundColor; need to first convert colorspace." Hence the
  // conversion first to CGColor.
  CGColorRef cg_color = [color CGColor];
  const size_t component_count = CGColorGetNumberOfComponents(cg_color);
  if (component_count == 4)
    return skia::CGColorRefToSkColor(cg_color);

  CHECK(component_count == 1 || component_count == 2);
  // 1-2 components means a grayscale channel and maybe an alpha channel, which
  // CGColorRefToSkColor will not like. But RGB is additive, so the conversion
  // is easy (RGB to grayscale is less easy).
  const CGFloat* components = CGColorGetComponents(cg_color);
  CGFloat alpha = component_count == 2 ? components[1] : 1.0;
  return SkColorSetARGB(SkScalarRoundToInt(255.0 * alpha),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]));
}

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeMac::instance();
}

// static
NativeThemeMac* NativeThemeMac::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeMac, s_native_theme, ());
  return &s_native_theme;
}

SkColor NativeThemeMac::GetSystemColor(ColorId color_id) const {
  // TODO(tapted): Add caching for these, and listen for
  // NSSystemColorsDidChangeNotification.
  switch (color_id) {
    case kColorId_WindowBackground:
      return NSSystemColorToSkColor([NSColor windowBackgroundColor]);
    case kColorId_DialogBackground:
      return kDialogBackgroundColor;
    case kColorId_BubbleBackground:
      return SK_ColorWHITE;

    case kColorId_FocusedBorderColor:
    case kColorId_FocusedMenuButtonBorderColor:
      return NSSystemColorToSkColor([NSColor keyboardFocusIndicatorColor]);
    case kColorId_UnfocusedBorderColor:
      return NSSystemColorToSkColor([NSColor controlColor]);

    // Buttons and labels.
    case kColorId_ButtonBackgroundColor:
    case kColorId_ButtonHoverBackgroundColor:
    case kColorId_HoverMenuButtonBorderColor:
    case kColorId_LabelBackgroundColor:
      return NSSystemColorToSkColor([NSColor controlBackgroundColor]);
    case kColorId_ButtonEnabledColor:
    case kColorId_EnabledMenuButtonBorderColor:
    case kColorId_LabelEnabledColor:
      return NSSystemColorToSkColor([NSColor controlTextColor]);
    case kColorId_ButtonDisabledColor:
    case kColorId_LabelDisabledColor:
      return NSSystemColorToSkColor([NSColor disabledControlTextColor]);
    case kColorId_ButtonHighlightColor:
    case kColorId_ButtonHoverColor:
      return NSSystemColorToSkColor([NSColor selectedControlTextColor]);

    // Menus.
    case kColorId_EnabledMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor controlTextColor]);
    case kColorId_DisabledMenuItemForegroundColor:
    case kColorId_DisabledEmphasizedMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor disabledControlTextColor]);
    case kColorId_SelectedMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor selectedMenuItemTextColor]);
    case kColorId_FocusedMenuItemBackgroundColor:
    case kColorId_HoverMenuItemBackgroundColor:
      return NSSystemColorToSkColor([NSColor selectedMenuItemColor]);
    case kColorId_MenuBackgroundColor:
      return kMenuPopupBackgroundColor;
    case kColorId_MenuSeparatorColor:
      return kMenuSeparatorColor;
    case kColorId_MenuBorderColor:
      return kMenuBorderColor;

    // Link.
    case kColorId_LinkDisabled:
      return SK_ColorBLACK;
    case kColorId_LinkEnabled:
      return SK_ColorBLUE;
    case kColorId_LinkPressed:
      return SK_ColorRED;

    // Text fields.
    case kColorId_TextfieldDefaultColor:
    case kColorId_TextfieldReadOnlyColor:
      return NSSystemColorToSkColor([NSColor textColor]);
    case kColorId_TextfieldDefaultBackground:
    case kColorId_TextfieldReadOnlyBackground:
      return NSSystemColorToSkColor([NSColor textBackgroundColor]);
    case kColorId_TextfieldSelectionColor:
      return NSSystemColorToSkColor([NSColor selectedTextColor]);
    case kColorId_TextfieldSelectionBackgroundFocused:
      return NSSystemColorToSkColor([NSColor selectedTextBackgroundColor]);

    // Trees/Tables. For focused text, use the alternate* versions, which
    // NSColor documents as "the table and list view equivalent to the
    // selectedControlTextColor".
    case kColorId_TreeBackground:
    case kColorId_TableBackground:
      return NSSystemColorToSkColor([NSColor controlBackgroundColor]);
    case kColorId_TreeText:
    case kColorId_TableText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TreeSelectedTextUnfocused:
      return NSSystemColorToSkColor([NSColor textColor]);
    case kColorId_TreeSelectedText:
    case kColorId_TableSelectedText:
      return NSSystemColorToSkColor(
          [NSColor alternateSelectedControlTextColor]);
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundFocused:
      return NSSystemColorToSkColor([NSColor alternateSelectedControlColor]);
    case kColorId_TreeSelectionBackgroundUnfocused:
    case kColorId_TableSelectionBackgroundUnfocused:
      return kUnfocusedSelectedTextBackgroundColor;
    case kColorId_TreeArrow:
    case kColorId_TableGroupingIndicatorColor:
      return SkColorSetRGB(140, 140, 140);

    default:
      // TODO(tapted): Handle all values and remove the default case.
      return GetAuraColor(color_id, this);
  }
}

void NativeThemeMac::PaintScrollbarTrack(
    SkCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  // Emulate the non-overlay scroller style from OSX 10.7 and later.
  SkPoint gradient_bounds[2];
  if (part == kScrollbarVerticalTrack) {
    gradient_bounds[0].set(rect.x(), rect.y());
    gradient_bounds[1].set(rect.right(), rect.y());
  } else {
    DCHECK_EQ(part, kScrollbarHorizontalTrack);
    gradient_bounds[0].set(rect.x(), rect.y());
    gradient_bounds[1].set(rect.x(), rect.bottom());
  }
  skia::RefPtr<SkShader> shader = skia::AdoptRef(
      SkGradientShader::CreateLinear(gradient_bounds,
                                     kScrollerTrackGradientColors,
                                     NULL,
                                     arraysize(kScrollerTrackGradientColors),
                                     SkShader::kClamp_TileMode));
  SkPaint gradient;
  gradient.setShader(shader.get());

  SkIRect track_rect = gfx::RectToSkIRect(rect);
  canvas->drawIRect(track_rect, gradient);

  // Draw inner and outer line borders.
  if (part == kScrollbarVerticalTrack) {
    SkPaint paint;
    paint.setColor(kScrollerTrackInnerBorderColor);
    canvas->drawRectCoords(track_rect.left(),
                           track_rect.top(),
                           track_rect.left() + kScrollerTrackBorderWidth,
                           track_rect.bottom(),
                           paint);
    paint.setColor(kScrollerTrackOuterBorderColor);
    canvas->drawRectCoords(track_rect.right() - kScrollerTrackBorderWidth,
                           track_rect.top(),
                           track_rect.right(),
                           track_rect.bottom(),
                           paint);
  } else {
    SkPaint paint;
    paint.setColor(kScrollerTrackInnerBorderColor);
    canvas->drawRectCoords(track_rect.left(),
                           track_rect.top(),
                           track_rect.right(),
                           track_rect.top() + kScrollerTrackBorderWidth,
                           paint);
    paint.setColor(kScrollerTrackOuterBorderColor);
    canvas->drawRectCoords(track_rect.left(),
                           track_rect.bottom() - kScrollerTrackBorderWidth,
                           track_rect.right(),
                           track_rect.bottom(),
                           paint);
  }
}

void NativeThemeMac::PaintScrollbarThumb(SkCanvas* canvas,
                                         Part part,
                                         State state,
                                         const gfx::Rect& rect) const {
  gfx::Rect thumb_rect(rect);
  switch (part) {
    case kScrollbarHorizontalThumb:
      thumb_rect.Inset(0, kScrollerTrackBorderWidth, 0, 0);
      break;
    case kScrollbarVerticalThumb:
      thumb_rect.Inset(kScrollerTrackBorderWidth, 0, 0, 0);
      break;
    default:
      NOTREACHED();
      break;
  }

  thumb_rect.Inset(kScrollerThumbInset, kScrollerThumbInset);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(state == kHovered ? thumb_active_color_
                                   : thumb_inactive_color_);
  const SkScalar radius = std::min(rect.width(), rect.height());
  canvas->drawRoundRect(gfx::RectToSkRect(thumb_rect), radius, radius, paint);
}

void NativeThemeMac::PaintScrollbarCorner(SkCanvas* canvas,
                                          State state,
                                          const gfx::Rect& rect) const {
  DCHECK_GT(rect.width(), 0);
  DCHECK_GT(rect.height(), 0);

  // Draw radial gradient from top-left corner.
  skia::RefPtr<SkShader> shader = skia::AdoptRef(
      SkGradientShader::CreateRadial(SkPoint::Make(rect.x(), rect.y()),
                                     rect.width(),
                                     kScrollerTrackGradientColors,
                                     NULL,
                                     arraysize(kScrollerTrackGradientColors),
                                     SkShader::kClamp_TileMode));
  SkPaint gradient;
  gradient.setStyle(SkPaint::kFill_Style);
  gradient.setAntiAlias(true);
  gradient.setShader(shader.get());
  canvas->drawRect(gfx::RectToSkRect(rect), gradient);

  // Draw inner border corner point.
  canvas->drawPoint(rect.x(), rect.y(), kScrollerTrackInnerBorderColor);

  // Draw outer borders.
  SkPaint paint;
  paint.setColor(kScrollerTrackOuterBorderColor);
  canvas->drawRectCoords(rect.right() - kScrollerTrackBorderWidth,
                         rect.y(),
                         rect.right(),
                         rect.bottom(),
                         paint);
  canvas->drawRectCoords(rect.x(),
                         rect.bottom() - kScrollerTrackBorderWidth,
                         rect.right(),
                         rect.bottom(),
                         paint);
}

void NativeThemeMac::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  SkPaint paint;
  paint.setAntiAlias(true);
  if (base::mac::IsOSYosemiteOrLater())
    paint.setColor(kMenuPopupBackgroundColorYosemite);
  else
    paint.setColor(kMenuPopupBackgroundColor);
  const SkScalar radius = SkIntToScalar(menu_background.corner_radius);
  SkRect rect = gfx::RectToSkRect(gfx::Rect(size));
  canvas->drawRoundRect(rect, radius, radius, paint);
}

void NativeThemeMac::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  SkPaint paint;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      // Draw nothing over the regular background.
      break;
    case NativeTheme::kHovered:
      // TODO(tapted): Draw a gradient, and use [NSColor currentControlTint] to
      // pick colors. The System color "selectedMenuItemColor" is actually still
      // blue for Graphite. And while "keyboardFocusIndicatorColor" does change,
      // and is a good shade of gray, it's not blue enough for the Blue theme.
      paint.setColor(GetSystemColor(kColorId_HoverMenuItemBackgroundColor));
      canvas->drawRect(gfx::RectToSkRect(rect), paint);
      break;
    default:
      NOTREACHED();
      break;
  }
}

NativeThemeMac::NativeThemeMac() {
  set_scrollbar_button_length(0);
  SetScrollbarColors(kScrollerThumbColor,
                     kScrollerThumbHoverColor,
                     kScrollerTrackGradientColors[0]);
}

NativeThemeMac::~NativeThemeMac() {
}

}  // namespace ui
