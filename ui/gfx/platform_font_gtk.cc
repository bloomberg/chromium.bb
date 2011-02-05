// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_gtk.h"

#include <algorithm>
#include <fontconfig/fontconfig.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <map>
#include <pango/pango.h>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gtk_util.h"

namespace {

// The font family name which is used when a user's application font for
// GNOME/KDE is a non-scalable one. The name should be listed in the
// IsFallbackFontAllowed function in skia/ext/SkFontHost_fontconfig_direct.cpp.
const char* kFallbackFontFamilyName = "sans";

// Returns the number of pixels in a point.
// - multiply a point size by this to get pixels ("device units")
// - divide a pixel size by this to get points
float GetPixelsInPoint() {
  static float pixels_in_point = 1.0;
  static bool determined_value = false;

  if (!determined_value) {
    // http://goo.gl/UIh5m: "This is a scale factor between points specified in
    // a PangoFontDescription and Cairo units.  The default value is 96, meaning
    // that a 10 point font will be 13 units high. (10 * 96. / 72. = 13.3)."
    double pango_dpi = gfx::GetPangoResolution();
    if (pango_dpi <= 0)
      pango_dpi = 96.0;
    pixels_in_point = pango_dpi / 72.0;  // 72 points in an inch
    determined_value = true;
  }

  return pixels_in_point;
}

// Retrieves the pango metrics for a pango font description. Caches the metrics
// and never frees them. The metrics objects are relatively small and
// very expensive to look up.
PangoFontMetrics* GetPangoFontMetrics(PangoFontDescription* desc) {
  static std::map<int, PangoFontMetrics*>* desc_to_metrics = NULL;
  static PangoContext* context = NULL;

  if (!context) {
    context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    pango_context_set_language(context, pango_language_get_default());
  }

  if (!desc_to_metrics) {
    desc_to_metrics = new std::map<int, PangoFontMetrics*>();
  }

  int desc_hash = pango_font_description_hash(desc);
  std::map<int, PangoFontMetrics*>::iterator i =
      desc_to_metrics->find(desc_hash);

  if (i == desc_to_metrics->end()) {
    PangoFontMetrics* metrics = pango_context_get_metrics(context, desc, NULL);
    (*desc_to_metrics)[desc_hash] = metrics;
    return metrics;
  } else {
    return i->second;
  }
}

// Find the best match font for |family_name| in the same way as Skia
// to make sure CreateFont() successfully creates a default font.  In
// Skia, it only checks the best match font.  If it failed to find
// one, SkTypeface will be NULL for that font family.  It eventually
// causes a segfault.  For example, family_name = "Sans" and system
// may have various fonts.  The first font family in FcPattern will be
// "DejaVu Sans" but a font family returned by FcFontMatch will be "VL
// PGothic".  In this case, SkTypeface for "Sans" returns NULL even if
// the system has a font for "Sans" font family.  See FontMatch() in
// skia/ports/SkFontHost_fontconfig.cpp for more detail.
string16 FindBestMatchFontFamilyName(const char* family_name) {
  FcPattern* pattern = FcPatternCreate();
  FcValue fcvalue;
  fcvalue.type = FcTypeString;
  char* family_name_copy = strdup(family_name);
  fcvalue.u.s = reinterpret_cast<FcChar8*>(family_name_copy);
  FcPatternAdd(pattern, FC_FAMILY, fcvalue, 0);
  FcConfigSubstitute(0, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  DCHECK(match) << "Could not find font: " << family_name;
  FcChar8* match_family;
  FcPatternGetString(match, FC_FAMILY, 0, &match_family);

  string16 font_family = UTF8ToUTF16(reinterpret_cast<char*>(match_family));
  FcPatternDestroy(match);
  FcPatternDestroy(pattern);
  free(family_name_copy);
  return font_family;
}

}  // namespace

namespace gfx {

Font* PlatformFontGtk::default_font_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// PlatformFontGtk, public:

PlatformFontGtk::PlatformFontGtk() {
  if (default_font_ == NULL) {
    GtkSettings* settings = gtk_settings_get_default();

    gchar* font_name = NULL;
    g_object_get(settings, "gtk-font-name", &font_name, NULL);

    // Temporary CHECK for helping track down
    // http://code.google.com/p/chromium/issues/detail?id=12530
    CHECK(font_name) << " Unable to get gtk-font-name for default font.";

    PangoFontDescription* desc =
        pango_font_description_from_string(font_name);
    default_font_ = new Font(desc);
    pango_font_description_free(desc);
    g_free(font_name);

    DCHECK(default_font_);
  }

  InitFromPlatformFont(
      static_cast<PlatformFontGtk*>(default_font_->platform_font()));
}

PlatformFontGtk::PlatformFontGtk(const Font& other) {
  InitFromPlatformFont(
      static_cast<PlatformFontGtk*>(other.platform_font()));
}

PlatformFontGtk::PlatformFontGtk(NativeFont native_font) {
  const char* family_name = pango_font_description_get_family(native_font);

  gint size_in_pixels = 0;
  if (pango_font_description_get_size_is_absolute(native_font)) {
    // If the size is absolute, then it's in Pango units rather than points.
    // There are PANGO_SCALE Pango units in a device unit (pixel).
    size_in_pixels = pango_font_description_get_size(native_font) / PANGO_SCALE;
  } else {
    // Otherwise, we need to convert from points.
    size_in_pixels =
        pango_font_description_get_size(native_font) * GetPixelsInPoint() /
        PANGO_SCALE;
  }

  // Find best match font for |family_name| to make sure we can get
  // a SkTypeface for the default font.
  // TODO(agl): remove this.
  string16 font_family = FindBestMatchFontFamilyName(family_name);

  InitWithNameAndSize(font_family, size_in_pixels);
  int style = 0;
  if (pango_font_description_get_weight(native_font) == PANGO_WEIGHT_BOLD) {
    // TODO(davemoore) What should we do about other weights? We currently
    // only support BOLD.
    style |= gfx::Font::BOLD;
  }
  if (pango_font_description_get_style(native_font) == PANGO_STYLE_ITALIC) {
    // TODO(davemoore) What about PANGO_STYLE_OBLIQUE?
    style |= gfx::Font::ITALIC;
  }
  if (style != 0)
    style_ = style;
}

PlatformFontGtk::PlatformFontGtk(const string16& font_name,
                                 int font_size) {
  InitWithNameAndSize(font_name, font_size);
}

double PlatformFontGtk::underline_position() const {
  const_cast<PlatformFontGtk*>(this)->InitPangoMetrics();
  return underline_position_pixels_;
}

double PlatformFontGtk::underline_thickness() const {
  const_cast<PlatformFontGtk*>(this)->InitPangoMetrics();
  return underline_thickness_pixels_;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontGtk, PlatformFont implementation:

Font PlatformFontGtk::DeriveFont(int size_delta, int style) const {
  // If the delta is negative, if must not push the size below 1
  if (size_delta < 0)
    DCHECK_LT(-size_delta, font_size_pixels_);

  if (style == style_) {
    // Fast path, we just use the same typeface at a different size
    return Font(new PlatformFontGtk(typeface_,
                                    font_family_,
                                    font_size_pixels_ + size_delta,
                                    style_));
  }

  // If the style has changed we may need to load a new face
  int skstyle = SkTypeface::kNormal;
  if (gfx::Font::BOLD & style)
    skstyle |= SkTypeface::kBold;
  if (gfx::Font::ITALIC & style)
    skstyle |= SkTypeface::kItalic;

  SkTypeface* typeface = SkTypeface::CreateFromName(
      UTF16ToUTF8(font_family_).c_str(),
      static_cast<SkTypeface::Style>(skstyle));
  SkAutoUnref tf_helper(typeface);

  return Font(new PlatformFontGtk(typeface,
                                  font_family_,
                                  font_size_pixels_ + size_delta,
                                  style));
}

int PlatformFontGtk::GetHeight() const {
  return height_pixels_;
}

int PlatformFontGtk::GetBaseline() const {
  return ascent_pixels_;
}

int PlatformFontGtk::GetAverageCharacterWidth() const {
  return SkScalarRound(average_width_pixels_);
}

int PlatformFontGtk::GetStringWidth(const string16& text) const {
  int width = 0, height = 0;
  CanvasSkia::SizeStringInt(text, Font(const_cast<PlatformFontGtk*>(this)),
                            &width, &height, gfx::Canvas::NO_ELLIPSIS);
  return width;
}

int PlatformFontGtk::GetExpectedTextWidth(int length) const {
  double char_width = const_cast<PlatformFontGtk*>(this)->GetAverageWidth();
  return round(static_cast<float>(length) * char_width);
}

int PlatformFontGtk::GetStyle() const {
  return style_;
}

string16 PlatformFontGtk::GetFontName() const {
  return font_family_;
}

int PlatformFontGtk::GetFontSize() const {
  return font_size_pixels_;
}

NativeFont PlatformFontGtk::GetNativeFont() const {
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, UTF16ToUTF8(GetFontName()).c_str());
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
    case gfx::Font::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontGtk, private:

PlatformFontGtk::PlatformFontGtk(SkTypeface* typeface,
                                 const string16& name,
                                 int size,
                                 int style) {
  InitWithTypefaceNameSizeAndStyle(typeface, name, size, style);
}

PlatformFontGtk::~PlatformFontGtk() {}

void PlatformFontGtk::InitWithNameAndSize(const string16& font_name,
                                          int font_size) {
  DCHECK_GT(font_size, 0);
  string16 fallback;

  SkTypeface* typeface = SkTypeface::CreateFromName(
      UTF16ToUTF8(font_name).c_str(), SkTypeface::kNormal);
  if (!typeface) {
    // A non-scalable font such as .pcf is specified. Falls back to a default
    // scalable font.
    typeface = SkTypeface::CreateFromName(
        kFallbackFontFamilyName, SkTypeface::kNormal);
    CHECK(typeface) << "Could not find any font: "
                    << UTF16ToUTF8(font_name)
                    << ", " << kFallbackFontFamilyName;
    fallback = UTF8ToUTF16(kFallbackFontFamilyName);
  }
  SkAutoUnref typeface_helper(typeface);

  InitWithTypefaceNameSizeAndStyle(typeface,
                                   fallback.empty() ? font_name : fallback,
                                   font_size,
                                   gfx::Font::NORMAL);
}

void PlatformFontGtk::InitWithTypefaceNameSizeAndStyle(
    SkTypeface* typeface,
    const string16& font_family,
    int font_size,
    int style) {
  typeface_helper_.reset(new SkAutoUnref(typeface));
  typeface_ = typeface;
  typeface_->ref();
  font_family_ = font_family;
  font_size_pixels_ = font_size;
  style_ = style;
  pango_metrics_inited_ = false;
  average_width_pixels_ = 0.0f;
  underline_position_pixels_ = 0.0f;
  underline_thickness_pixels_ = 0.0f;

  SkPaint paint;
  SkPaint::FontMetrics metrics;
  PaintSetup(&paint);
  paint.getFontMetrics(&metrics);

  ascent_pixels_ = SkScalarCeil(-metrics.fAscent);
  height_pixels_ = ascent_pixels_ + SkScalarCeil(metrics.fDescent);
}

void PlatformFontGtk::InitFromPlatformFont(const PlatformFontGtk* other) {
  typeface_helper_.reset(new SkAutoUnref(other->typeface_));
  typeface_ = other->typeface_;
  typeface_->ref();
  font_family_ = other->font_family_;
  font_size_pixels_ = other->font_size_pixels_;
  style_ = other->style_;
  height_pixels_ = other->height_pixels_;
  ascent_pixels_ = other->ascent_pixels_;
  pango_metrics_inited_ = other->pango_metrics_inited_;
  average_width_pixels_ = other->average_width_pixels_;
  underline_position_pixels_ = other->underline_position_pixels_;
  underline_thickness_pixels_ = other->underline_thickness_pixels_;
}

void PlatformFontGtk::PaintSetup(SkPaint* paint) const {
  paint->setAntiAlias(false);
  paint->setSubpixelText(false);
  paint->setTextSize(font_size_pixels_);
  paint->setTypeface(typeface_);
  paint->setFakeBoldText((gfx::Font::BOLD & style_) && !typeface_->isBold());
  paint->setTextSkewX((gfx::Font::ITALIC & style_) && !typeface_->isItalic() ?
                      -SK_Scalar1/4 : 0);
}

void PlatformFontGtk::InitPangoMetrics() {
  if (!pango_metrics_inited_) {
    pango_metrics_inited_ = true;
    PangoFontDescription* pango_desc = GetNativeFont();
    PangoFontMetrics* pango_metrics = GetPangoFontMetrics(pango_desc);

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
    double pango_width_pixels =
        pango_font_metrics_get_approximate_char_width(pango_metrics) /
        PANGO_SCALE;

    // Yes, this is how Microsoft recommends calculating the dialog unit
    // conversions.
    int text_width_pixels = GetStringWidth(
        ASCIIToUTF16("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
    double dialog_units_pixels = (text_width_pixels / 26 + 1) / 2;
    average_width_pixels_ = std::min(pango_width_pixels, dialog_units_pixels);
    pango_font_description_free(pango_desc);
  }
}


double PlatformFontGtk::GetAverageWidth() const {
  const_cast<PlatformFontGtk*>(this)->InitPangoMetrics();
  return average_width_pixels_;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontGtk;
}

// static
PlatformFont* PlatformFont::CreateFromFont(const Font& other) {
  return new PlatformFontGtk(other);
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontGtk(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const string16& font_name,
                                                  int font_size) {
  return new PlatformFontGtk(font_name, font_size);
}

}  // namespace gfx
