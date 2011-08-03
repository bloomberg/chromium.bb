// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_H_
#define UI_GFX_RENDER_TEXT_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/range/range.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/point.h"

namespace {

// Strike line width.
const int kStrikeWidth = 2;

// Color settings for text, backgrounds and cursor.
// These are tentative, and should be derived from theme, system
// settings and current settings.
// TODO(oshima): Change this to match the standard chrome
// before dogfooding textfield views.
const SkColor kSelectedTextColor = SK_ColorWHITE;
const SkColor kFocusedSelectionColor = SK_ColorCYAN;
const SkColor kUnfocusedSelectionColor = SK_ColorLTGRAY;
const SkColor kCursorColor = SK_ColorBLACK;

}  // namespace

namespace gfx {

class Canvas;
class RenderTextTest;

// A visual style applicable to a range of text.
struct UI_API StyleRange {
  StyleRange();

  Font font;
  SkColor foreground;
  bool strike;
  bool underline;
  ui::Range range;
};

typedef std::vector<StyleRange> StyleRanges;

// TODO(msw): Distinguish between logical character and glyph?
enum BreakType {
  CHARACTER_BREAK,
  WORD_BREAK,
  LINE_BREAK,
};

// TODO(msw): Implement RenderText[Win|Linux] for Uniscribe/Pango BiDi...

// RenderText represents an abstract model of styled text and its corresponding
// visual layout. Support is built in for a cursor, a selection, simple styling,
// complex scripts, and bi-directional text. Implementations provide mechanisms
// for rendering and translation between logical and visual data.
class UI_API RenderText {

 public:
  virtual ~RenderText();

  // Creates a platform-specific RenderText instance.
  static RenderText* CreateRenderText();

  const string16& text() const { return text_; }
  virtual void SetText(const string16& text);

  bool cursor_visible() const { return cursor_visible_; }
  void set_cursor_visible(bool visible) { cursor_visible_ = visible; }

  bool insert_mode() const { return insert_mode_; }
  void toggle_insert_mode() { insert_mode_ = !insert_mode_; }

  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }

  const StyleRange& default_style() const { return default_style_; }
  void set_default_style(StyleRange style) { default_style_ = style; }

  const Rect& display_rect() const { return display_rect_; }
  void set_display_rect(const Rect& r) { display_rect_ = r; }

  const gfx::Point& display_offset() const { return display_offset_; }

  size_t GetCursorPosition() const;
  void SetCursorPosition(const size_t position);

  // Moves the cursor left or right. Cursor movement is visual, meaning that
  // left and right are relative to screen, not the directionality of the text.
  // If |select| is false, the selection range is emptied at the new position.
  // If |break_type| is CHARACTER_BREAK, move to the neighboring character.
  // If |break_type| is WORD_BREAK, move to the nearest word boundary.
  // If |break_type| is LINE_BREAK, move to text edge as shown on screen.
  void MoveCursorLeft(BreakType break_type, bool select);
  void MoveCursorRight(BreakType break_type, bool select);

  // Moves the cursor to the specified logical |position|.
  // If |select| is false, the selection range is emptied at the new position.
  // Returns true if the cursor position or selection range changed.
  bool MoveCursorTo(size_t position, bool select);

  // Move the cursor to the position associated with the clicked point.
  // If |select| is false, the selection range is emptied at the new position.
  bool MoveCursorTo(const Point& point, bool select);

  const ui::Range& GetSelection() const;
  void SetSelection(const ui::Range& range);

  // Returns true if the local point is over selected text.
  bool IsPointInSelection(const Point& point) const;

  // Selects no text, all text, or the word at the current cursor position.
  void ClearSelection();
  void SelectAll();
  void SelectWord();

  const ui::Range& GetCompositionRange() const;
  void SetCompositionRange(const ui::Range& composition_range);

  // Apply |style_range| to the internal style model.
  virtual void ApplyStyleRange(StyleRange style_range);

  // Apply |default_style_| over the entire text range.
  virtual void ApplyDefaultStyle();

  base::i18n::TextDirection GetTextDirection() const;

  // Get the width of the entire string.
  int GetStringWidth() const;

  virtual void Draw(Canvas* canvas);

  // TODO(msw): Deprecate this function. Logical and visual cursors are not
  //  mapped one-to-one. See the selection_range_ TODO for more information.
  // Get the logical cursor position from a visual point in local coordinates.
  virtual size_t FindCursorPosition(const Point& point) const;

  // Get the visual bounds containing the logical substring within |range|.
  // These bounds could be visually discontiguous if the logical selection range
  // is split by an odd number of LTR/RTL level change.
  virtual std::vector<Rect> GetSubstringBounds(
      const ui::Range& range) const;

  // Get the visual bounds describing the cursor at |position|. These bounds
  // typically represent a vertical line, but if |insert_mode| is true they
  // contain the bounds of the associated glyph.
  virtual Rect GetCursorBounds(size_t position, bool insert_mode) const;

 protected:
  RenderText();

  const StyleRanges& style_ranges() const { return style_ranges_; }

  // Get the cursor position that visually neighbors |position|.
  // If |move_by_word| is true, return the neighboring word delimiter position.
  virtual size_t GetLeftCursorPosition(size_t position,
                                       bool move_by_word) const;
  virtual size_t GetRightCursorPosition(size_t position,
                                        bool move_by_word) const;

 private:
  friend class RenderTextTest;

  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, DefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, CustomDefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ApplyStyleRange);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, StyleRangesAdjust);

  // Clear out |style_ranges_|.
  void ClearStyleRanges();

  bool IsPositionAtWordSelectionBoundary(size_t pos);

  // Logical UTF-16 string data to be drawn.
  string16 text_;

  // TODO(msw): A single logical cursor position doesn't support two potential
  //  visual cursor positions. For example, clicking right of 'c' & 'D' yeilds:
  //  (visually: 'abc|FEDghi' and 'abcFED|ghi', both logically: 'abc|DEFghi').
  //  Similarly, one visual position may have two associated logical positions.
  //  For example, clicking the right side of 'D' and left side of 'g' yields:
  //  (both visually: 'abcFED|ghi', logically: 'abc|DEFghi' and 'abcDEF|ghi').
  //  Update the cursor model with a leading/trailing flag, a level association,
  //  or a disjoint visual position to satisfy the proposed visual behavior.
  // Logical selection range; the range end is also the logical cursor position.
  ui::Range selection_range_;

  // The cursor visibility and insert mode.
  bool cursor_visible_;
  bool insert_mode_;

  // The focus state of the text.
  bool focused_;

  // Composition text range.
  ui::Range composition_range_;

  // List of style ranges. Elements in the list never overlap each other.
  StyleRanges style_ranges_;
  // The default text style.
  StyleRange default_style_;

  // The local display area for rendering the text.
  Rect display_rect_;
  // The offset for the text to be drawn, relative to the display area.
  Point display_offset_;

  DISALLOW_COPY_AND_ASSIGN(RenderText);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_H_
