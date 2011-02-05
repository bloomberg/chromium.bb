// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_win.h"

#include <windows.h>
#include <math.h>

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"

namespace {

// If the tmWeight field of a TEXTMETRIC structure has a value >= this, the
// font is bold.
const int kTextMetricWeightBold = 700;

// Returns either minimum font allowed for a current locale or
// lf_height + size_delta value.
int AdjustFontSize(int lf_height, int size_delta) {
  if (lf_height < 0) {
    lf_height -= size_delta;
  } else {
    lf_height += size_delta;
  }
  int min_font_size = 0;
  if (gfx::PlatformFontWin::get_minimum_font_size_callback)
    min_font_size = gfx::PlatformFontWin::get_minimum_font_size_callback();
  // Make sure lf_height is not smaller than allowed min font size for current
  // locale.
  if (abs(lf_height) < min_font_size) {
    return lf_height < 0 ? -min_font_size : min_font_size;
  } else {
    return lf_height;
  }
}

}  // namespace

namespace gfx {

// static
PlatformFontWin::HFontRef* PlatformFontWin::base_font_ref_;

// static
PlatformFontWin::AdjustFontCallback
    PlatformFontWin::adjust_font_callback = NULL;
PlatformFontWin::GetMinimumFontSizeCallback
    PlatformFontWin::get_minimum_font_size_callback = NULL;

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin, public

PlatformFontWin::PlatformFontWin() : font_ref_(GetBaseFontRef()) {
}

PlatformFontWin::PlatformFontWin(const Font& other) {
  InitWithCopyOfHFONT(other.GetNativeFont());
}

PlatformFontWin::PlatformFontWin(NativeFont native_font) {
  InitWithCopyOfHFONT(native_font);
}

PlatformFontWin::PlatformFontWin(const string16& font_name,
                                 int font_size) {
  InitWithFontNameAndSize(font_name, font_size);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin, PlatformFont implementation:

Font PlatformFontWin::DeriveFont(int size_delta, int style) const {
  LOGFONT font_info;
  GetObject(GetNativeFont(), sizeof(LOGFONT), &font_info);
  font_info.lfHeight = AdjustFontSize(font_info.lfHeight, size_delta);
  font_info.lfUnderline =
      ((style & gfx::Font::UNDERLINED) == gfx::Font::UNDERLINED);
  font_info.lfItalic = ((style & gfx::Font::ITALIC) == gfx::Font::ITALIC);
  font_info.lfWeight = (style & gfx::Font::BOLD) ? FW_BOLD : FW_NORMAL;

  HFONT hfont = CreateFontIndirect(&font_info);
  return Font(new PlatformFontWin(CreateHFontRef(hfont)));
}

int PlatformFontWin::GetHeight() const {
  return font_ref_->height();
}

int PlatformFontWin::GetBaseline() const {
  return font_ref_->baseline();
}

int PlatformFontWin::GetAverageCharacterWidth() const {
  return font_ref_->ave_char_width();
}

int PlatformFontWin::GetStringWidth(const string16& text) const {
  int width = 0, height = 0;
  CanvasSkia::SizeStringInt(text, Font(const_cast<PlatformFontWin*>(this)),
                            &width, &height, gfx::Canvas::NO_ELLIPSIS);
  return width;
}

int PlatformFontWin::GetExpectedTextWidth(int length) const {
  return length * std::min(font_ref_->dlu_base_x(), GetAverageCharacterWidth());
}

int PlatformFontWin::GetStyle() const {
  return font_ref_->style();
}

string16 PlatformFontWin::GetFontName() const {
  return font_ref_->font_name();
}

int PlatformFontWin::GetFontSize() const {
  LOGFONT font_info;
  GetObject(font_ref_->hfont(), sizeof(LOGFONT), &font_info);
  long lf_height = font_info.lfHeight;
  HDC hdc = GetDC(NULL);
  int device_caps = GetDeviceCaps(hdc, LOGPIXELSY);
  int font_size = 0;
  if (device_caps != 0) {
    float font_size_float = -static_cast<float>(lf_height)*72/device_caps;
    font_size = static_cast<int>(::ceil(font_size_float - 0.5));
  }
  ReleaseDC(NULL, hdc);
  return font_size;
}

NativeFont PlatformFontWin::GetNativeFont() const {
  return font_ref_->hfont();
}

////////////////////////////////////////////////////////////////////////////////
// Font, private:

void PlatformFontWin::InitWithCopyOfHFONT(HFONT hfont) {
  DCHECK(hfont);
  LOGFONT font_info;
  GetObject(hfont, sizeof(LOGFONT), &font_info);
  font_ref_ = CreateHFontRef(CreateFontIndirect(&font_info));
}

void PlatformFontWin::InitWithFontNameAndSize(const string16& font_name,
                                              int font_size) {
  HDC hdc = GetDC(NULL);
  long lf_height = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ReleaseDC(NULL, hdc);
  HFONT hf = ::CreateFont(lf_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          font_name.c_str());
  font_ref_ = CreateHFontRef(hf);
}

// static
PlatformFontWin::HFontRef* PlatformFontWin::GetBaseFontRef() {
  if (base_font_ref_ == NULL) {
    NONCLIENTMETRICS metrics;
    base::win::GetNonClientMetrics(&metrics);

    if (adjust_font_callback)
      adjust_font_callback(&metrics.lfMessageFont);
    metrics.lfMessageFont.lfHeight =
        AdjustFontSize(metrics.lfMessageFont.lfHeight, 0);
    HFONT font = CreateFontIndirect(&metrics.lfMessageFont);
    DLOG_ASSERT(font);
    base_font_ref_ = PlatformFontWin::CreateHFontRef(font);
    // base_font_ref_ is global, up the ref count so it's never deleted.
    base_font_ref_->AddRef();
  }
  return base_font_ref_;
}

PlatformFontWin::HFontRef* PlatformFontWin::CreateHFontRef(HFONT font) {
  TEXTMETRIC font_metrics;
  HDC screen_dc = GetDC(NULL);
  HFONT previous_font = static_cast<HFONT>(SelectObject(screen_dc, font));
  int last_map_mode = SetMapMode(screen_dc, MM_TEXT);
  GetTextMetrics(screen_dc, &font_metrics);
  // Yes, this is how Microsoft recommends calculating the dialog unit
  // conversions.
  SIZE ave_text_size;
  GetTextExtentPoint32(screen_dc,
                       L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                       52, &ave_text_size);
  const int dlu_base_x = (ave_text_size.cx / 26 + 1) / 2;
  // To avoid the DC referencing font_handle_, select the previous font.
  SelectObject(screen_dc, previous_font);
  SetMapMode(screen_dc, last_map_mode);
  ReleaseDC(NULL, screen_dc);

  const int height = std::max(1, static_cast<int>(font_metrics.tmHeight));
  const int baseline = std::max(1, static_cast<int>(font_metrics.tmAscent));
  const int ave_char_width =
      std::max(1, static_cast<int>(font_metrics.tmAveCharWidth));
  int style = 0;
  if (font_metrics.tmItalic)
    style |= Font::ITALIC;
  if (font_metrics.tmUnderlined)
    style |= Font::UNDERLINED;
  if (font_metrics.tmWeight >= kTextMetricWeightBold)
    style |= Font::BOLD;

  return new HFontRef(font, height, baseline, ave_char_width, style,
                      dlu_base_x);
}

PlatformFontWin::PlatformFontWin(HFontRef* hfont_ref) : font_ref_(hfont_ref) {
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin::HFontRef:

PlatformFontWin::HFontRef::HFontRef(HFONT hfont,
         int height,
         int baseline,
         int ave_char_width,
         int style,
         int dlu_base_x)
    : hfont_(hfont),
      height_(height),
      baseline_(baseline),
      ave_char_width_(ave_char_width),
      style_(style),
      dlu_base_x_(dlu_base_x) {
  DLOG_ASSERT(hfont);

  LOGFONT font_info;
  GetObject(hfont_, sizeof(LOGFONT), &font_info);
  font_name_ = string16(font_info.lfFaceName);
}

PlatformFontWin::HFontRef::~HFontRef() {
  DeleteObject(hfont_);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontWin;
}

// static
PlatformFont* PlatformFont::CreateFromFont(const Font& other) {
  return new PlatformFontWin(other);
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontWin(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const string16& font_name,
                                                  int font_size) {
  return new PlatformFontWin(font_name, font_size);
}

}  // namespace gfx
