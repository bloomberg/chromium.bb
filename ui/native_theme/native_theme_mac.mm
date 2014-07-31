// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/native_theme/common_theme.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/gfx/skia_util.h"

namespace {

// Values calculated by reading pixels and solving simultaneous equations
// derived from "A over B" alpha compositing. Steps: Sample the semi-transparent
// pixel over two backgrounds; P1, P2 over backgrounds B1, B2. Use the color
// value between 0.0 and 1.0 (i.e. divide by 255.0). Then,
// alpha = (P2 - P1 + B1 - B2) / (B1 - B2)
// color = (P1 - B1 + alpha * B1) / alpha.
const SkColor kMenuPopupBackgroundColor = SkColorSetARGB(251, 255, 255, 255);
const SkColor kMenuSeparatorColor = SkColorSetARGB(243, 228, 228, 228);
const SkColor kMenuBorderColor = SkColorSetARGB(60, 0, 0, 0);

// Hardcoded color used for some existing dialogs in Chrome's Cocoa UI.
const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);

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
  COMPILE_ASSERT(sizeof(swatch) == bytes_per_row, skcolor_not_4_bytes);
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
    return gfx::NSDeviceColorToSkColor(device_color);

  // Sometimes the conversion is not possible, but we can get an approximation
  // by going through a CGColorRef. Note that simply using NSColor methods for
  // accessing components for system colors results in exceptions like
  // "-numberOfComponents not valid for the NSColor NSNamedColorSpace System
  // windowBackgroundColor; need to first convert colorspace." Hence the
  // conversion first to CGColor.
  CGColorRef cg_color = [color CGColor];
  if (CGColorGetNumberOfComponents(cg_color) == 4)
    return gfx::CGColorRefToSkColor(cg_color);

  CHECK_EQ(2u, CGColorGetNumberOfComponents(cg_color));
  // Two components means a grayscale channel and an alpha channel, which
  // CGColorRefToSkColor will not like. But RGB is additive, so the conversion
  // is easy (RGB to grayscale is less easy).
  const CGFloat* components = CGColorGetComponents(cg_color);
  return SkColorSetARGB(SkScalarRoundToInt(255.0 * components[1]),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]));
}

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::instance() {
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

    default:
      break;  // TODO(tapted): Handle all values and remove the default case.
  }

  SkColor color;
  if (CommonThemeGetSystemColor(color_id, &color))
    return color;

  NOTIMPLEMENTED() << " Invalid color_id: " << color_id;
  return FallbackTheme::GetSystemColor(color_id);
}

void NativeThemeMac::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  canvas->drawColor(kMenuPopupBackgroundColor, SkXfermode::kSrc_Mode);
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
}

NativeThemeMac::~NativeThemeMac() {
}

}  // namespace ui
