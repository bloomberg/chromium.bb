// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_WIN_
#define UI_GFX_PLATFORM_FONT_WIN_
#pragma once

#include "base/ref_counted.h"
#include "gfx/platform_font.h"

namespace gfx {

class PlatformFontWin : public PlatformFont {
 public:
  PlatformFontWin();
  explicit PlatformFontWin(const Font& other);
  explicit PlatformFontWin(NativeFont native_font);
  PlatformFontWin(const string16& font_name,
                  int font_size);

  // Dialog units to pixels conversion.
  // See http://support.microsoft.com/kb/145994 for details.
  int horizontal_dlus_to_pixels(int dlus) const {
    return dlus * font_ref_->dlu_base_x() / 4;
  }
  int vertical_dlus_to_pixels(int dlus)  const {
    return dlus * font_ref_->height() / 8;
  }

  // Callback that returns the minimum height that should be used for
  // gfx::Fonts. Optional. If not specified, the minimum font size is 0.
  typedef int (*GetMinimumFontSizeCallback)();
  static GetMinimumFontSizeCallback get_minimum_font_size_callback;

  // Callback that adjusts a LOGFONT to meet suitability requirements of the
  // embedding application. Optional. If not specified, no adjustments are
  // performed other than clamping to a minimum font height if
  // |get_minimum_font_size_callback| is specified.
  typedef void (*AdjustFontCallback)(LOGFONT* lf);
  static AdjustFontCallback adjust_font_callback;

  // Overridden from PlatformFont:
  virtual Font DeriveFont(int size_delta, int style) const;
  virtual int GetHeight() const;
  virtual int GetBaseline() const;
  virtual int GetAverageCharacterWidth() const;
  virtual int GetStringWidth(const string16& text) const;
  virtual int GetExpectedTextWidth(int length) const;
  virtual int GetStyle() const;
  virtual string16 GetFontName() const;
  virtual int GetFontSize() const;
  virtual NativeFont GetNativeFont() const;

 private:
  virtual ~PlatformFontWin() {}

  // Chrome text drawing bottoms out in the Windows GDI functions that take an
  // HFONT (an opaque handle into Windows). To avoid lots of GDI object
  // allocation and destruction, Font indirectly refers to the HFONT by way of
  // an HFontRef. That is, every Font has an HFontRef, which has an HFONT.
  //
  // HFontRef is reference counted. Upon deletion, it deletes the HFONT.
  // By making HFontRef maintain the reference to the HFONT, multiple
  // HFontRefs can share the same HFONT, and Font can provide value semantics.
  class HFontRef : public base::RefCounted<HFontRef> {
   public:
    // This constructor takes control of the HFONT, and will delete it when
    // the HFontRef is deleted.
    HFontRef(HFONT hfont,
             int height,
             int baseline,
             int ave_char_width,
             int style,
             int dlu_base_x);

    // Accessors
    HFONT hfont() const { return hfont_; }
    int height() const { return height_; }
    int baseline() const { return baseline_; }
    int ave_char_width() const { return ave_char_width_; }
    int style() const { return style_; }
    int dlu_base_x() const { return dlu_base_x_; }
    const string16& font_name() const { return font_name_; }

   private:
    friend class  base::RefCounted<HFontRef>;

    ~HFontRef();

    const HFONT hfont_;
    const int height_;
    const int baseline_;
    const int ave_char_width_;
    const int style_;
    // Constants used in converting dialog units to pixels.
    const int dlu_base_x_;
    string16 font_name_;

    DISALLOW_COPY_AND_ASSIGN(HFontRef);
  };

  // Initializes this object with a copy of the specified HFONT.
  void InitWithCopyOfHFONT(HFONT hfont);

  // Initializes this object with the specified font name and size.
  void InitWithFontNameAndSize(const string16& font_name,
                               int font_size);

  // Returns the base font ref. This should ONLY be invoked on the
  // UI thread.
  static HFontRef* GetBaseFontRef();

  // Creates and returns a new HFONTRef from the specified HFONT.
  static HFontRef* CreateHFontRef(HFONT font);

  // Creates a new PlatformFontWin with the specified HFontRef. Used when
  // constructing a Font from a HFONT we don't want to copy.
  explicit PlatformFontWin(HFontRef* hfont_ref);

    // Reference to the base font all fonts are derived from.
  static HFontRef* base_font_ref_;

  // Indirect reference to the HFontRef, which references the underlying HFONT.
  scoped_refptr<HFontRef> font_ref_;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_WIN_

