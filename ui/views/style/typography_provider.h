// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_
#define UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/views/views_export.h"

namespace gfx {
class FontList;
}

namespace ui {
class NativeTheme;
}

namespace views {

// Provides fonts to use in toolkit-views UI.
class VIEWS_EXPORT TypographyProvider {
 public:
  virtual ~TypographyProvider() = default;

  // Gets the FontList for the given |context| and |style|.
  virtual const gfx::FontList& GetFont(int context, int style) const = 0;

  // Gets the color for the given |context| and |style|, optionally consulting
  // |theme|.
  virtual SkColor GetColor(int context,
                           int style,
                           const ui::NativeTheme& theme) const = 0;

  // Gets the line spacing, or 0 if it should be provided by gfx::FontList.
  virtual int GetLineHeight(int context, int style) const = 0;

  // The system may indicate a "bold" UI font is preferred (e.g. by selecting
  // the "Bold" checkbox in Windows under "Change only the text size" in
  // Control Panel). In this case, a user's gfx::Weight::NORMAL font will
  // already be bold, and requesting a MEDIUM font will result in a font that is
  // less bold. So this method returns NORMAL, if the NORMAL font is at least as
  // bold as |weight|.
  static gfx::Font::Weight WeightNotLighterThanNormal(gfx::Font::Weight weight);

 protected:
  TypographyProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TypographyProvider);
};

// The default provider aims to provide values to match pre-Harmony constants
// for the given contexts so that old UI does not change.
class VIEWS_EXPORT DefaultTypographyProvider : public TypographyProvider {
 public:
  DefaultTypographyProvider() = default;

  // TypographyProvider:
  const gfx::FontList& GetFont(int context, int style) const override;
  SkColor GetColor(int context,
                   int style,
                   const ui::NativeTheme& theme) const override;
  int GetLineHeight(int context, int style) const override;

  // Sets the |size_delta| and |font_weight| that the the default GetFont()
  // implementation uses. Always sets values, even for styles it doesn't know
  // about.
  static void GetDefaultFont(int context,
                             int style,
                             int* size_delta,
                             gfx::Font::Weight* font_weight);

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultTypographyProvider);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_TYPOGRAPHY_PROVIDER_H_
