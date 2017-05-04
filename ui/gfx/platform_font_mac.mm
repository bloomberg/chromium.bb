// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <cmath>

#include <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"

namespace gfx {

namespace {

// Returns the font style for |font|. Disregards Font::UNDERLINE, since NSFont
// does not support it as a trait.
int GetFontStyleFromNSFont(NSFont* font) {
  int font_style = Font::NORMAL;
  NSFontSymbolicTraits traits = [[font fontDescriptor] symbolicTraits];
  if (traits & NSFontItalicTrait)
    font_style |= Font::ITALIC;
  return font_style;
}

// Returns the Font weight for |font|.
Font::Weight GetFontWeightFromNSFont(NSFont* font) {
  NSFontSymbolicTraits traits = [[font fontDescriptor] symbolicTraits];
  return (traits & NSFontBoldTrait) ? Font::Weight::BOLD : Font::Weight::NORMAL;
}

// Returns an autoreleased NSFont created with the passed-in specifications.
NSFont* NSFontWithSpec(const std::string& font_name,
                       int font_size,
                       int font_style,
                       Font::Weight font_weight) {
  NSFontSymbolicTraits trait_bits = 0;
  // TODO(mboc): Add support for other weights as well.
  if (font_weight >= Font::Weight::BOLD)
    trait_bits |= NSFontBoldTrait;
  if (font_style & Font::ITALIC)
    trait_bits |= NSFontItalicTrait;
  // The Mac doesn't support underline as a font trait, so just drop it.
  // (Underlines must be added as an attribute on an NSAttributedString.)
  NSDictionary* traits = @{ NSFontSymbolicTrait : @(trait_bits) };

  NSDictionary* attrs = @{
    NSFontFamilyAttribute : base::SysUTF8ToNSString(font_name),
    NSFontTraitsAttribute : traits
  };
  NSFontDescriptor* descriptor =
      [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];
  NSFont* font = [NSFont fontWithDescriptor:descriptor size:font_size];
  if (font)
    return font;

  // Make one fallback attempt by looking up via font name rather than font
  // family name.
  attrs = @{
    NSFontNameAttribute : base::SysUTF8ToNSString(font_name),
    NSFontTraitsAttribute : traits
  };
  descriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];
  return [NSFont fontWithDescriptor:descriptor size:font_size];
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, public:

PlatformFontMac::PlatformFontMac()
    : PlatformFontMac([NSFont systemFontOfSize:[NSFont systemFontSize]]) {
}

PlatformFontMac::PlatformFontMac(NativeFont native_font)
    : PlatformFontMac(native_font,
                      base::SysNSStringToUTF8([native_font familyName]),
                      [native_font pointSize],
                      GetFontStyleFromNSFont(native_font),
                      GetFontWeightFromNSFont(native_font)) {}

PlatformFontMac::PlatformFontMac(const std::string& font_name, int font_size)
    : PlatformFontMac(font_name,
                      font_size,
                      Font::NORMAL,
                      Font::Weight::NORMAL) {}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, PlatformFont implementation:

Font PlatformFontMac::DeriveFont(int size_delta,
                                 int style,
                                 Font::Weight weight) const {
  // For some reason, creating fonts using the NSFontDescriptor API's seem to be
  // unreliable. Hence use the NSFontManager.
  NSFont* derived_font = native_font_;
  NSFontManager* font_manager = [NSFontManager sharedFontManager];

  NSFontTraitMask bold_trait_mask =
      weight >= Font::Weight::BOLD ? NSBoldFontMask : NSUnboldFontMask;
  derived_font =
      [font_manager convertFont:derived_font toHaveTrait:bold_trait_mask];

  NSFontTraitMask italic_trait_mask =
      (style & Font::ITALIC) ? NSItalicFontMask : NSUnitalicFontMask;
  derived_font =
      [font_manager convertFont:derived_font toHaveTrait:italic_trait_mask];

  derived_font =
      [font_manager convertFont:derived_font toSize:font_size_ + size_delta];

  return Font(new PlatformFontMac(derived_font, font_name_,
                                  font_size_ + size_delta, style, weight));
}

int PlatformFontMac::GetHeight() {
  return height_;
}

int PlatformFontMac::GetBaseline() {
  return ascent_;
}

int PlatformFontMac::GetCapHeight() {
  return cap_height_;
}

int PlatformFontMac::GetExpectedTextWidth(int length) {
  if (!average_width_ && native_font_) {
    // -[NSFont boundingRectForGlyph:] seems to always return the largest
    // bounding rect that could be needed, which produces very wide expected
    // widths for strings. Instead, compute the actual width of a string
    // containing all the lowercase characters to find a reasonable guess at the
    // average.
    base::scoped_nsobject<NSAttributedString> attr_string(
        [[NSAttributedString alloc]
            initWithString:@"abcdefghijklmnopqrstuvwxyz"
                attributes:@{NSFontAttributeName : native_font_.get()}]);
    average_width_ = [attr_string size].width / [attr_string length];
    DCHECK_NE(0, average_width_);
  }
  return ceil(length * average_width_);
}

int PlatformFontMac::GetStyle() const {
  return font_style_;
}

Font::Weight PlatformFontMac::GetWeight() const {
  return font_weight_;
}

const std::string& PlatformFontMac::GetFontName() const {
  return font_name_;
}

std::string PlatformFontMac::GetActualFontNameForTesting() const {
  return base::SysNSStringToUTF8([native_font_ familyName]);
}

int PlatformFontMac::GetFontSize() const {
  return font_size_;
}

const FontRenderParams& PlatformFontMac::GetFontRenderParams() {
  return render_params_;
}

NativeFont PlatformFontMac::GetNativeFont() const {
  return [[native_font_.get() retain] autorelease];
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, private:

PlatformFontMac::PlatformFontMac(const std::string& font_name,
                                 int font_size,
                                 int font_style,
                                 Font::Weight font_weight)
    : PlatformFontMac(
          NSFontWithSpec(font_name, font_size, font_style, font_weight),
          font_name,
          font_size,
          font_style,
          font_weight) {}

PlatformFontMac::PlatformFontMac(NativeFont font,
                                 const std::string& font_name,
                                 int font_size,
                                 int font_style,
                                 Font::Weight font_weight)
    : native_font_([font retain]),
      font_name_(font_name),
      font_size_(font_size),
      font_style_(font_style),
      font_weight_(font_weight) {
  CalculateMetricsAndInitRenderParams();
}

PlatformFontMac::~PlatformFontMac() {
}

void PlatformFontMac::CalculateMetricsAndInitRenderParams() {
  NSFont* font = native_font_.get();
  if (!font) {
    // This object was constructed from a font name that doesn't correspond to
    // an actual font. Don't waste time working out metrics.
    height_ = 0;
    ascent_ = 0;
    cap_height_ = 0;
    return;
  }

  ascent_ = ceil([font ascender]);
  cap_height_ = ceil([font capHeight]);

  // PlatformFontMac once used -[NSLayoutManager defaultLineHeightForFont:] to
  // initialize |height_|. However, it has a silly rounding bug. Essentially, it
  // gives round(ascent) + round(descent). E.g. Helvetica Neue at size 16 gives
  // ascent=15.4634, descent=3.38208 -> 15 + 3 = 18. When the height should be
  // at least 19. According to the OpenType specification, these values should
  // simply be added, so do that. Note this uses the already-rounded |ascent_|
  // to ensure GetBaseline() + descender fits within GetHeight() during layout.
  height_ = ceil(ascent_ + std::abs([font descender]) + [font leading]);

  FontRenderParamsQuery query;
  query.families.push_back(font_name_);
  query.pixel_size = font_size_;
  query.style = font_style_;
  query.weight = font_weight_;
  render_params_ = gfx::GetFontRenderParams(query, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontMac;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontMac(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontMac(font_name, font_size);
}

}  // namespace gfx
