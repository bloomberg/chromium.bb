// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_H_
#define UI_GFX_RENDER_TEXT_H_
#pragma once

#include <algorithm>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/range/range.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/point.h"

namespace gfx {

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

class Canvas;
class RenderTextTest;

// A visual style applicable to a range of text.
struct UI_EXPORT StyleRange {
  StyleRange();

  Font font;
  SkColor foreground;
  bool strike;
  bool underline;
  ui::Range range;
};

typedef std::vector<StyleRange> StyleRanges;

// TODO(msw): Distinguish between logical character stops and glyph stops?
// CHARACTER_BREAK cursor movements should stop at neighboring characters.
// WORD_BREAK cursor movements should stop at the nearest word boundaries.
// LINE_BREAK cursor movements should stop at the text ends as shown on screen.
enum BreakType {
  CHARACTER_BREAK,
  WORD_BREAK,
  LINE_BREAK,
};

// TODO(xji): publish bidi-editing guide line and replace the place holder.
// SelectionModel is used to represent the logical selection and visual
// position of cursor.
//
// For bi-directional text, the mapping between visual position and logical
// position is not one-to-one. For example, logical text "abcDEF" where capital
// letters stand for Hebrew, the visual display is "abcFED". According to the
// bidi editing guide (http://bidi-editing-guideline):
// 1. If pointing to the right half of the cell of a LTR character, the current
// position must be set after this character and the caret must be displayed
// after this character.
// 2. If pointing to the right half of the cell of a RTL character, the current
// position must be set before this character and the caret must be displayed
// before this character.
//
// Pointing to the right half of 'c' and pointing to the right half of 'D' both
// set the logical cursor position to 3. But the cursor displayed visually at
// different places:
// Pointing to the right half of 'c' displays the cursor right of 'c' as
// "abc|FED".
// Pointing to the right half of 'D' displays the cursor right of 'D' as
// "abcFED|".
// So, besides the logical selection start point and end point, we need extra
// information to specify to which character and on which edge of the character
// the visual cursor is bound to. For example, the visual cursor is bound to
// the trailing side of the 2nd character 'c' when pointing to right half of
// 'c'. And it is bound to the leading edge of the 3rd character 'D' when
// pointing to right of 'D'.
class UI_EXPORT SelectionModel {
 public:
  enum CaretPlacement {
    LEADING,
    TRAILING,
  };

  SelectionModel();
  explicit SelectionModel(size_t pos);
  SelectionModel(size_t start, size_t end);
  SelectionModel(size_t end, size_t pos, CaretPlacement status);
  SelectionModel(size_t start, size_t end, size_t pos, CaretPlacement status);

  virtual ~SelectionModel();

  size_t selection_start() const { return selection_start_; }
  void set_selection_start(size_t pos) { selection_start_ = pos; }

  size_t selection_end() const { return selection_end_; }
  void set_selection_end(size_t pos) { selection_end_ = pos; }

  size_t caret_pos() const { return caret_pos_; }
  void set_caret_pos(size_t pos) { caret_pos_ = pos; }

  CaretPlacement caret_placement() const { return caret_placement_; }
  void set_caret_placement(CaretPlacement placement) {
    caret_placement_ = placement;
  }

  bool Equals(const SelectionModel& sel) const;

 private:
  void Init(size_t start, size_t end, size_t pos, CaretPlacement status);

  // Logical selection start. If there is non-empty selection, if
  // selection_start_ is less than selection_end_, the selection starts visually
  // at the leading edge of the selection_start_. If selection_start_ is greater
  // than selection_end_, the selection starts visually at the trailing edge of
  // selection_start_'s previous grapheme. So, we do not need extra information
  // for visual bounding.
  size_t selection_start_;

  // The logical cursor position that next character will be inserted into.
  // It is also the end of the selection.
  size_t selection_end_;

  // The following two fields are used to guide cursor visual position.
  // The index of the character that cursor is visually attached to.
  size_t caret_pos_;
  // The visual placement of the cursor, relative to its associated character.
  CaretPlacement caret_placement_;
};

// TODO(msw): Implement RenderText[Win|Linux] for Uniscribe/Pango BiDi...

// RenderText represents an abstract model of styled text and its corresponding
// visual layout. Support is built in for a cursor, a selection, simple styling,
// complex scripts, and bi-directional text. Implementations provide mechanisms
// for rendering and translation between logical and visual data.
class UI_EXPORT RenderText {
 public:
  virtual ~RenderText();

  // Creates a platform-specific RenderText instance.
  static RenderText* CreateRenderText();

  const string16& text() const { return text_; }
  virtual void SetText(const string16& text);

  const SelectionModel& selection_model() const { return selection_model_; }

  bool cursor_visible() const { return cursor_visible_; }
  void set_cursor_visible(bool visible) { cursor_visible_ = visible; }

  bool insert_mode() const { return insert_mode_; }
  void ToggleInsertMode();

  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }

  const StyleRange& default_style() const { return default_style_; }
  void set_default_style(StyleRange style) { default_style_ = style; }

  const Rect& display_rect() const { return display_rect_; }
  virtual void SetDisplayRect(const Rect& r);

  // This cursor position corresponds to SelectionModel::selection_end. In
  // addition to representing the selection end, it's also where logical text
  // edits take place, and doesn't necessarily correspond to
  // SelectionModel::caret_pos.
  size_t GetCursorPosition() const;
  void SetCursorPosition(size_t position);

  void SetCaretPlacement(SelectionModel::CaretPlacement placement) {
      selection_model_.set_caret_placement(placement);
  }

  // Moves the cursor left or right. Cursor movement is visual, meaning that
  // left and right are relative to screen, not the directionality of the text.
  // If |select| is false, the selection start is moved to the same position.
  void MoveCursorLeft(BreakType break_type, bool select);
  void MoveCursorRight(BreakType break_type, bool select);

  // Set the selection_model_ to the value of |selection|.
  // The selection model components are modified if invalid.
  // Returns true if the cursor position or selection range changed.
  // TODO(xji): need to check the cursor is set at grapheme boundary.
  bool MoveCursorTo(const SelectionModel& selection_model);

  // Move the cursor to the position associated with the clicked point.
  // If |select| is false, the selection start is moved to the same position.
  // Returns true if the cursor position or selection range changed.
  bool MoveCursorTo(const Point& point, bool select);

  size_t GetSelectionStart() const {
      return selection_model_.selection_start();
  }
  size_t MinOfSelection() const {
      return std::min(GetSelectionStart(), GetCursorPosition());
  }
  size_t MaxOfSelection() const {
      return std::max(GetSelectionStart(), GetCursorPosition());
  }
  bool EmptySelection() const {
      return GetSelectionStart() == GetCursorPosition();
  }

  // Returns true if the local point is over selected text.
  bool IsPointInSelection(const Point& point);

  // Selects no text, all text, or the word at the current cursor position.
  void ClearSelection();
  void SelectAll();
  void SelectWord();

  const ui::Range& GetCompositionRange() const;
  virtual void SetCompositionRange(const ui::Range& composition_range);

  // Apply |style_range| to the internal style model.
  virtual void ApplyStyleRange(StyleRange style_range);

  // Apply |default_style_| over the entire text range.
  virtual void ApplyDefaultStyle();

  virtual base::i18n::TextDirection GetTextDirection();

  // Get the width of the entire string.
  virtual int GetStringWidth();

  virtual void Draw(Canvas* canvas);

  // Gets the SelectionModel from a visual point in local coordinates.
  virtual SelectionModel FindCursorPosition(const Point& point);

  // Get the visual bounds of a cursor at |selection|. These bounds typically
  // represent a vertical line, but if |insert_mode| is true they contain the
  // bounds of the associated glyph. These bounds are in local coordinates, but
  // may be outside the visible region if the text is longer than the textfield.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  virtual Rect GetCursorBounds(const SelectionModel& selection,
                               bool insert_mode);

  // Compute the current cursor bounds, panning the text to show the cursor in
  // the display rect if necessary. These bounds are in local coordinates.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  const Rect& GetUpdatedCursorBounds();

 protected:
  RenderText();

  const Point& GetUpdatedDisplayOffset();

  void set_cached_bounds_and_offset_valid(bool valid) {
      cached_bounds_and_offset_valid_ = valid;
  }

  const StyleRanges& style_ranges() const { return style_ranges_; }

  // Get the selection model that visually neighbors |position| by |break_type|.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel GetLeftSelectionModel(const SelectionModel& current,
                                               BreakType break_type);
  virtual SelectionModel GetRightSelectionModel(const SelectionModel& current,
                                                BreakType break_type);

  // Get the SelectionModels corresponding to visual text ends.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel LeftEndSelectionModel();
  virtual SelectionModel RightEndSelectionModel();

  // Get the logical index of the grapheme preceeding the argument |position|.
  virtual size_t GetIndexOfPreviousGrapheme(size_t position);

  // Get the visual bounds containing the logical substring within |from| to
  // |to|. These bounds could be visually discontinuous if the substring is
  // split by a LTR/RTL level change. These bounds are in local coordinates, but
  // may be outside the visible region if the text is longer than the textfield.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  // TODO(msw) Re-evaluate this function's necessity and signature.
  virtual std::vector<Rect> GetSubstringBounds(size_t from, size_t to);

  // Apply composition style (underline) to composition range and selection
  // style (foreground) to selection range.
  void ApplyCompositionAndSelectionStyles(StyleRanges* style_ranges) const;

  // Convert points from the text space to the view space and back.
  // Handles the display area, display offset, and the application LTR/RTL mode.
  Point ToTextPoint(const Point& point);
  Point ToViewPoint(const Point& point);

 private:
  friend class RenderTextTest;

  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, DefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, CustomDefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ApplyStyleRange);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, StyleRangesAdjust);

  // Sets the selection model, the argument is assumed to be valid.
  void SetSelectionModel(const SelectionModel& selection_model);

  // Set the cursor to |position|, with the caret trailing the previous
  // grapheme, or if there is no previous grapheme, leading the cursor position.
  // If |select| is false, the selection start is moved to the same position.
  void MoveCursorTo(size_t position, bool select);

  bool IsPositionAtWordSelectionBoundary(size_t pos);

  // Update the cached bounds and display offset to ensure that the current
  // cursor is within the visible display area.
  void UpdateCachedBoundsAndOffset();

  // Returns the selection model of selection_start_.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetSelectionModelForSelectionStart();

  // Logical UTF-16 string data to be drawn.
  string16 text_;

  // Logical selection range and visual cursor position.
  SelectionModel selection_model_;

  // The cached cursor bounds; get these bounds with GetUpdatedCursorBounds.
  Rect cursor_bounds_;

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
  // Get this point with GetUpdatedDisplayOffset (or risk using a stale value).
  Point display_offset_;

  // The cached bounds and offset are invalidated by changes to the cursor,
  // selection, font, and other operations that adjust the visible text bounds.
  bool cached_bounds_and_offset_valid_;

  DISALLOW_COPY_AND_ASSIGN(RenderText);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_H_
