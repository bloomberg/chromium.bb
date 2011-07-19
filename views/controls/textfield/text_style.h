// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXT_STYLE_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXT_STYLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class Font;
}

namespace views {

// A class that specifies text style for TextfieldViews.
// TODO(suzhe|oshima): support underline color and thick style.
class TextStyle {
 public:
  // Foreground color for the text.
  void set_foreground(SkColor color) { foreground_ = color; }

  // Draws diagnoal strike acrosss the text.
  void set_strike(bool strike) { strike_ = strike; }

  // Adds underline to the text.
  void set_underline(bool underline) { underline_ = underline; }

 private:
  friend class NativeTextfieldViews;
  friend class TextfieldViewsModel;

  FRIEND_TEST_ALL_PREFIXES(TextfieldViewsModelTest, TextStyleTest);
  FRIEND_TEST_ALL_PREFIXES(TextfieldViewsModelTest, UndoRedo_CompositionText);
  FRIEND_TEST_ALL_PREFIXES(TextfieldViewsModelTest, CompositionTextTest);

  TextStyle();
  virtual ~TextStyle();

  SkColor foreground() const { return foreground_; }
  bool underline() const { return underline_; }

  // Draw string to the canvas within the region given
  // by |x|,|y|,|width|,|height|.
  void DrawString(gfx::Canvas* canvas,
                  string16& text,
                  gfx::Font& base_font,
                  bool read_only,
                  int x, int y, int width, int height) const;

  SkColor foreground_;
  bool strike_;
  bool underline_;

  DISALLOW_COPY_AND_ASSIGN(TextStyle);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXT_STYLE_H_
