// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_pango.h"

#include <fontconfig/fontconfig.h>
#include <pango/pango.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/gfx/pango_util.h"
#include "ui/gfx/text_utils.h"

namespace {

// The font family name which is used when a user's application font for
// GNOME/KDE is a non-scalable one. The name should be listed in the
// IsFallbackFontAllowed function in skia/ext/SkFontHost_fontconfig_direct.cpp.
const char* kFallbackFontFamilyName = "sans";

// Creates a SkTypeface for the passed-in Font::FontStyle and family. If a
// fallback typeface is used instead of the requested family, |family| will be
// updated to contain the fallback's family name.
skia::RefPtr<SkTypeface> CreateSkTypeface(int style, std::string* family) {
  DCHECK(family);

  int skia_style = SkTypeface::kNormal;
  if (gfx::Font::BOLD & style)
    skia_style |= SkTypeface::kBold;
  if (gfx::Font::ITALIC & style)
    skia_style |= SkTypeface::kItalic;

  skia::RefPtr<SkTypeface> typeface = skia::AdoptRef(SkTypeface::CreateFromName(
      family->c_str(), static_cast<SkTypeface::Style>(skia_style)));
  if (!typeface) {
    // A non-scalable font such as .pcf is specified. Fall back to a default
    // scalable font.
    typeface = skia::AdoptRef(SkTypeface::CreateFromName(
        kFallbackFontFamilyName, static_cast<SkTypeface::Style>(skia_style)));
    CHECK(typeface) << "Could not find any font: " << family << ", "
                    << kFallbackFontFamilyName;
    *family = kFallbackFontFamilyName;
  }
  return typeface;
}

}  // namespace

namespace gfx {

// static
Font* PlatformFontPango::default_font_ = NULL;

#if defined(OS_CHROMEOS)
// static
std::string* PlatformFontPango::default_font_description_ = NULL;
#endif

////////////////////////////////////////////////////////////////////////////////
// PlatformFontPango, public:

PlatformFontPango::PlatformFontPango() {
  if (!default_font_) {
    scoped_ptr<ScopedPangoFontDescription> description;
#if defined(OS_CHROMEOS)
    CHECK(default_font_description_);
    description.reset(
        new ScopedPangoFontDescription(*default_font_description_));
#else
    const gfx::LinuxFontDelegate* delegate = gfx::LinuxFontDelegate::instance();
    if (delegate)
      description = delegate->GetDefaultPangoFontDescription();
#endif
    if (!description || !description->get())
      description.reset(new ScopedPangoFontDescription("sans 10"));
    default_font_ = new Font(description->get());
  }

  InitFromPlatformFont(
      static_cast<PlatformFontPango*>(default_font_->platform_font()));
}

PlatformFontPango::PlatformFontPango(NativeFont native_font) {
  std::string font_family;
  std::vector<std::string> family_names;
  base::SplitString(pango_font_description_get_family(native_font), ',',
                    &family_names);

  const int pango_size =
      pango_font_description_get_size(native_font) / PANGO_SCALE;
  const bool pango_using_pixels =
      pango_font_description_get_size_is_absolute(native_font);

  int style = 0;
  // TODO(davemoore) What should we do about other weights? We currently only
  // support BOLD.
  if (pango_font_description_get_weight(native_font) == PANGO_WEIGHT_BOLD)
    style |= gfx::Font::BOLD;
  // TODO(davemoore) What about PANGO_STYLE_OBLIQUE?
  if (pango_font_description_get_style(native_font) == PANGO_STYLE_ITALIC)
    style |= gfx::Font::ITALIC;

  const FontRenderParams params = GetCustomFontRenderParams(
      false, &family_names,
      pango_using_pixels ? &pango_size : NULL /* pixel_size */,
      !pango_using_pixels ? &pango_size : NULL /* point_size */,
      &style, &font_family);

  InitFromDetails(skia::RefPtr<SkTypeface>(), font_family,
                  gfx::GetPangoFontSizeInPixels(native_font), style, params);
}

PlatformFontPango::PlatformFontPango(const std::string& font_name,
                                     int font_size_pixels) {
  const std::vector<std::string> font_list(1, font_name);
  const int style = Font::NORMAL;
  const FontRenderParams params = GetCustomFontRenderParams(
      false, &font_list, &font_size_pixels, NULL, &style, NULL);
  InitFromDetails(skia::RefPtr<SkTypeface>(), font_name, font_size_pixels,
                  style, params);
}

double PlatformFontPango::underline_position() const {
  const_cast<PlatformFontPango*>(this)->InitPangoMetrics();
  return underline_position_pixels_;
}

double PlatformFontPango::underline_thickness() const {
  const_cast<PlatformFontPango*>(this)->InitPangoMetrics();
  return underline_thickness_pixels_;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontPango, PlatformFont implementation:

// static
void PlatformFontPango::ReloadDefaultFont() {
  delete default_font_;
  default_font_ = NULL;
}

#if defined(OS_CHROMEOS)
// static
void PlatformFontPango::SetDefaultFontDescription(
    const std::string& font_description) {
  delete default_font_description_;
  default_font_description_ = new std::string(font_description);
}

#endif

Font PlatformFontPango::DeriveFont(int size_delta, int style) const {
  const int new_size = font_size_pixels_ + size_delta;
  DCHECK_GT(new_size, 0);

  // If the style changed, we may need to load a new face.
  std::string new_family = font_family_;
  skia::RefPtr<SkTypeface> typeface =
      (style == style_) ? typeface_ : CreateSkTypeface(style, &new_family);

  const std::vector<std::string> family_list(1, new_family);
  const FontRenderParams render_params = GetCustomFontRenderParams(
      false, &family_list, &new_size, NULL, &style, NULL);

  return Font(new PlatformFontPango(typeface,
                                    new_family,
                                    new_size,
                                    style,
                                    render_params));
}

int PlatformFontPango::GetHeight() const {
  return height_pixels_;
}

int PlatformFontPango::GetBaseline() const {
  return ascent_pixels_;
}

int PlatformFontPango::GetCapHeight() const {
  return cap_height_pixels_;
}

int PlatformFontPango::GetExpectedTextWidth(int length) const {
  double char_width = const_cast<PlatformFontPango*>(this)->GetAverageWidth();
  return round(static_cast<float>(length) * char_width);
}

int PlatformFontPango::GetStyle() const {
  return style_;
}

std::string PlatformFontPango::GetFontName() const {
  return font_family_;
}

std::string PlatformFontPango::GetActualFontNameForTesting() const {
  SkString family_name;
  typeface_->getFamilyName(&family_name);
  return family_name.c_str();
}

int PlatformFontPango::GetFontSize() const {
  return font_size_pixels_;
}

const FontRenderParams& PlatformFontPango::GetFontRenderParams() const {
  return font_render_params_;
}

NativeFont PlatformFontPango::GetNativeFont() const {
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, GetFontName().c_str());
  // Set the absolute size to avoid overflowing UI elements.
  // pango_font_description_set_absolute_size() takes a size in Pango units.
  // There are PANGO_SCALE Pango units in one device unit.  Screen output
  // devices use pixels as their device units.
  pango_font_description_set_absolute_size(
      pfd, font_size_pixels_ * PANGO_SCALE);

  switch (GetStyle()) {
    case gfx::Font::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case gfx::Font::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case gfx::Font::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case gfx::Font::UNDERLINE:
      // TODO(deanm): How to do underline?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontPango, private:

PlatformFontPango::PlatformFontPango(const skia::RefPtr<SkTypeface>& typeface,
                                     const std::string& name,
                                     int size_pixels,
                                     int style,
                                     const FontRenderParams& render_params) {
  InitFromDetails(typeface, name, size_pixels, style, render_params);
}

PlatformFontPango::~PlatformFontPango() {}

void PlatformFontPango::InitFromDetails(
    const skia::RefPtr<SkTypeface>& typeface,
    const std::string& font_family,
    int font_size_pixels,
    int style,
    const FontRenderParams& render_params) {
  DCHECK_GT(font_size_pixels, 0);

  font_family_ = font_family;
  typeface_ = typeface ? typeface : CreateSkTypeface(style, &font_family_);

  font_size_pixels_ = font_size_pixels;
  style_ = style;
  font_render_params_ = render_params;

  SkPaint paint;
  SkPaint::FontMetrics metrics;
  PaintSetup(&paint);
  paint.getFontMetrics(&metrics);
  ascent_pixels_ = SkScalarCeilToInt(-metrics.fAscent);
  height_pixels_ = ascent_pixels_ + SkScalarCeilToInt(metrics.fDescent);
  cap_height_pixels_ = SkScalarCeilToInt(metrics.fCapHeight);

  pango_metrics_inited_ = false;
  average_width_pixels_ = 0.0f;
  underline_position_pixels_ = 0.0f;
  underline_thickness_pixels_ = 0.0f;
}

void PlatformFontPango::InitFromPlatformFont(const PlatformFontPango* other) {
  typeface_ = other->typeface_;
  font_family_ = other->font_family_;
  font_size_pixels_ = other->font_size_pixels_;
  style_ = other->style_;
  font_render_params_ = other->font_render_params_;
  ascent_pixels_ = other->ascent_pixels_;
  height_pixels_ = other->height_pixels_;
  cap_height_pixels_ = other->cap_height_pixels_;
  pango_metrics_inited_ = other->pango_metrics_inited_;
  average_width_pixels_ = other->average_width_pixels_;
  underline_position_pixels_ = other->underline_position_pixels_;
  underline_thickness_pixels_ = other->underline_thickness_pixels_;
}

void PlatformFontPango::PaintSetup(SkPaint* paint) const {
  paint->setAntiAlias(false);
  paint->setSubpixelText(false);
  paint->setTextSize(font_size_pixels_);
  paint->setTypeface(typeface_.get());
  paint->setFakeBoldText((gfx::Font::BOLD & style_) && !typeface_->isBold());
  paint->setTextSkewX((gfx::Font::ITALIC & style_) && !typeface_->isItalic() ?
                      -SK_Scalar1/4 : 0);
}

void PlatformFontPango::InitPangoMetrics() {
  if (!pango_metrics_inited_) {
    pango_metrics_inited_ = true;
    ScopedPangoFontDescription pango_desc(GetNativeFont());
    PangoFontMetrics* pango_metrics = GetPangoFontMetrics(pango_desc.get());

    underline_position_pixels_ =
        pango_font_metrics_get_underline_position(pango_metrics) /
        PANGO_SCALE;

    // TODO(davemoore): Come up with a better solution.
    // This is a hack, but without doing this the underlines
    // we get end up fuzzy. So we align to the midpoint of a pixel.
    underline_position_pixels_ /= 2;

    underline_thickness_pixels_ =
        pango_font_metrics_get_underline_thickness(pango_metrics) /
        PANGO_SCALE;

    // First get the Pango-based width (converting from Pango units to pixels).
    const double pango_width_pixels =
        pango_font_metrics_get_approximate_char_width(pango_metrics) /
        PANGO_SCALE;

    // Yes, this is how Microsoft recommends calculating the dialog unit
    // conversions.
    const int text_width_pixels = GetStringWidth(
        base::ASCIIToUTF16(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"),
        FontList(Font(this)));
    const double dialog_units_pixels = (text_width_pixels / 26 + 1) / 2;
    average_width_pixels_ = std::min(pango_width_pixels, dialog_units_pixels);
  }
}

double PlatformFontPango::GetAverageWidth() const {
  const_cast<PlatformFontPango*>(this)->InitPangoMetrics();
  return average_width_pixels_;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontPango;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontPango(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontPango(font_name, font_size);
}

}  // namespace gfx
