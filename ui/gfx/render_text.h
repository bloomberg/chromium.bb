// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_H_
#define UI_GFX_RENDER_TEXT_H_
#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/range/range.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/selection_model.h"

class SkCanvas;
class SkDrawLooper;
struct SkPoint;
class SkShader;
class SkTypeface;

namespace gfx {

class Canvas;
class Font;
class RenderTextTest;
class ShadowValue;
struct StyleRange;

namespace internal {

// Internal helper class used by derived classes to draw text through Skia.
class SkiaTextRenderer {
 public:
  explicit SkiaTextRenderer(Canvas* canvas);
  ~SkiaTextRenderer();

  void SetDrawLooper(SkDrawLooper* draw_looper);
  void SetFontSmoothingSettings(bool enable_smoothing, bool enable_lcd_text);
  void SetTypeface(SkTypeface* typeface);
  void SetTextSize(int size);
  void SetFontFamilyWithStyle(const std::string& family, int font_style);
  void SetForegroundColor(SkColor foreground);
  void SetShader(SkShader* shader, const Rect& bounds);
  void DrawSelection(const std::vector<Rect>& selection, SkColor color);
  void DrawPosText(const SkPoint* pos,
                   const uint16* glyphs,
                   size_t glyph_count);
  void DrawDecorations(int x, int y, int width, const StyleRange& style);

 private:
  SkCanvas* canvas_skia_;
  bool started_drawing_;
  SkPaint paint_;
  SkRect bounds_;
  SkRefPtr<SkShader> deferred_fade_shader_;

  DISALLOW_COPY_AND_ASSIGN(SkiaTextRenderer);
};

}  // namespace internal

// A visual style applicable to a range of text.
struct UI_EXPORT StyleRange {
  StyleRange();

  SkColor foreground;
  // A gfx::Font::FontStyle flag to specify bold and italic styles.
  int font_style;
  bool strike;
  bool diagonal_strike;
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

// Horizontal text alignment styles.
enum HorizontalAlignment {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT,
};

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
  void SetText(const string16& text);

  HorizontalAlignment horizontal_alignment() const {
    return horizontal_alignment_;
  }
  void SetHorizontalAlignment(HorizontalAlignment alignment);

  const FontList& font_list() const { return font_list_; }
  void SetFontList(const FontList& font_list);

  // Set the font size to |size| in pixels.
  void SetFontSize(int size);

  // Get the first font in |font_list_|.
  const Font& GetFont() const;

  bool cursor_enabled() const { return cursor_enabled_; }
  void SetCursorEnabled(bool cursor_enabled);

  bool cursor_visible() const { return cursor_visible_; }
  void set_cursor_visible(bool visible) { cursor_visible_ = visible; }

  bool insert_mode() const { return insert_mode_; }
  void ToggleInsertMode();

  SkColor cursor_color() const { return cursor_color_; }
  void set_cursor_color(SkColor color) { cursor_color_ = color; }

  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }

  const StyleRange& default_style() const { return default_style_; }
  void set_default_style(const StyleRange& style) { default_style_ = style; }

  // In an obscured (password) field, all text is drawn as asterisks or bullets.
  bool is_obscured() const { return obscured_; }
  void SetObscured(bool obscured);

  const Rect& display_rect() const { return display_rect_; }
  void SetDisplayRect(const Rect& r);

  void set_fade_head(bool fade_head) { fade_head_ = fade_head; }
  bool fade_head() const { return fade_head_; }
  void set_fade_tail(bool fade_tail) { fade_tail_ = fade_tail; }
  bool fade_tail() const { return fade_tail_; }

  bool background_is_transparent() const { return background_is_transparent_; }
  void set_background_is_transparent(bool transparent) {
    background_is_transparent_ = transparent;
  }

  const SelectionModel& selection_model() const { return selection_model_; }

  const ui::Range& selection() const { return selection_model_.selection(); }

  size_t cursor_position() const { return selection_model_.caret_pos(); }
  void SetCursorPosition(size_t position);

  // Moves the cursor left or right. Cursor movement is visual, meaning that
  // left and right are relative to screen, not the directionality of the text.
  // If |select| is false, the selection start is moved to the same position.
  void MoveCursor(BreakType break_type,
                  VisualCursorDirection direction,
                  bool select);

  // Set the selection_model_ to the value of |selection|.
  // The selection range is clamped to text().length() if out of range.
  // Returns true if the cursor position or selection range changed.
  // If any index in |selection_model| is not a cursorable position (not on a
  // grapheme boundary), it is a no-op and returns false.
  bool MoveCursorTo(const SelectionModel& selection_model);

  // Move the cursor to the position associated with the clicked point.
  // If |select| is false, the selection start is moved to the same position.
  // Returns true if the cursor position or selection range changed.
  bool MoveCursorTo(const Point& point, bool select);

  // Set the selection_model_ based on |range|.
  // If the |range| start or end is greater than text length, it is modified
  // to be the text length.
  // If the |range| start or end is not a cursorable position (not on grapheme
  // boundary), it is a NO-OP and returns false. Otherwise, returns true.
  bool SelectRange(const ui::Range& range);

  // Returns true if the local point is over selected text.
  bool IsPointInSelection(const Point& point);

  // Selects no text, all text, or the word at the current cursor position.
  void ClearSelection();
  void SelectAll();
  void SelectWord();

  const ui::Range& GetCompositionRange() const;
  void SetCompositionRange(const ui::Range& composition_range);

  // Apply |style_range| to the internal style model.
  void ApplyStyleRange(const StyleRange& style_range);

  // Apply |default_style_| over the entire text range.
  void ApplyDefaultStyle();

  // Returns the dominant direction of the current text.
  virtual base::i18n::TextDirection GetTextDirection() = 0;

  // Returns the visual movement direction corresponding to the logical end
  // of the text, considering only the dominant direction returned by
  // |GetTextDirection()|, not the direction of a particular run.
  VisualCursorDirection GetVisualDirectionOfLogicalEnd();

  // Get the size in pixels of the entire string. For the height, this will
  // return the maximum height among the different fonts in the text runs.
  // Note that this returns the raw size of the string, which does not include
  // the margin area of text shadows.
  virtual Size GetStringSize() = 0;

  void Draw(Canvas* canvas);

  // Gets the SelectionModel from a visual point in local coordinates.
  virtual SelectionModel FindCursorPosition(const Point& point) = 0;

  // Get the visual bounds of a cursor at |selection|. These bounds typically
  // represent a vertical line, but if |insert_mode| is true they contain the
  // bounds of the associated glyph. These bounds are in local coordinates, but
  // may be outside the visible region if the text is longer than the textfield.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  Rect GetCursorBounds(const SelectionModel& selection, bool insert_mode);

  // Compute the current cursor bounds, panning the text to show the cursor in
  // the display rect if necessary. These bounds are in local coordinates.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  const Rect& GetUpdatedCursorBounds();

  // Given an |index| in text(), return the next or previous grapheme boundary
  // in logical order (that is, the nearest index for which
  // |IsCursorablePosition(index)| returns true). The return value is in the
  // range 0 to text().length() inclusive (the input is clamped if it is out of
  // that range). Always moves by at least one character index unless the
  // supplied index is already at the boundary of the string.
  virtual size_t IndexOfAdjacentGrapheme(size_t index,
                                         LogicalCursorDirection direction) = 0;

  // Return a SelectionModel with the cursor at the current selection's start.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetSelectionModelForSelectionStart();

  // Sets shadows to drawn with text.
  void SetTextShadows(const std::vector<ShadowValue>& shadows);

  typedef std::pair<Font, ui::Range> FontSpan;
  // For testing purposes, returns which fonts were chosen for which parts of
  // the text by returning a vector of Font and Range pairs, where each range
  // specifies the character range for which the corresponding font has been
  // chosen.
  virtual std::vector<FontSpan> GetFontSpansForTesting() = 0;

 protected:
  RenderText();

  const Point& GetUpdatedDisplayOffset();

  void set_cached_bounds_and_offset_valid(bool valid) {
    cached_bounds_and_offset_valid_ = valid;
  }

  const StyleRanges& style_ranges() const { return style_ranges_; }

  // Get the selection model that visually neighbors |position| by |break_type|.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetAdjacentSelectionModel(const SelectionModel& current,
                                           BreakType break_type,
                                           VisualCursorDirection direction);

  // Get the selection model visually left/right of |selection| by one grapheme.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the selection model visually left/right of |selection| by one word.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the SelectionModels corresponding to visual text ends.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel EdgeSelectionModel(VisualCursorDirection direction);

  // Sets the selection model, the argument is assumed to be valid.
  virtual void SetSelectionModel(const SelectionModel& model);

  // Get the height and horizontal bounds (relative to the left of the text, not
  // the view) of the glyph starting at |index|. If the glyph is RTL then
  // xspan->is_reversed(). This does not return a Rect because a Rect can't have
  // a negative width.
  virtual void GetGlyphBounds(size_t index, ui::Range* xspan, int* height) = 0;

  // Get the visual bounds containing the logical substring within the |range|.
  // If |range| is empty, the result is empty. These bounds could be visually
  // discontinuous if the substring is split by a LTR/RTL level change.
  // These bounds are in local coordinates, but may be outside the visible
  // region if the text is longer than the textfield. Subsequent text, cursor,
  // or bounds changes may invalidate returned values.
  virtual std::vector<Rect> GetSubstringBounds(ui::Range range) = 0;

  // Return true if cursor can appear in front of the character at |position|,
  // which means it is a grapheme boundary or the first character in the text.
  virtual bool IsCursorablePosition(size_t position) = 0;

  // Reset the layout to be invalid.
  virtual void ResetLayout() = 0;

  // Ensure the text is laid out.
  virtual void EnsureLayout() = 0;

  // Draw the text.
  virtual void DrawVisualText(Canvas* canvas) = 0;

  // Like text() except that it returns asterisks or bullets if this is an
  // obscured field.
  string16 GetDisplayText() const;

  // Apply composition style (underline) to composition range and selection
  // style (foreground) to selection range.
  void ApplyCompositionAndSelectionStyles(StyleRanges* style_ranges);

  // Returns the text origin after applying text alignment and display offset.
  Point GetTextOrigin();

  // Convert points from the text space to the view space and back.
  // Handles the display area, display offset, and the application LTR/RTL mode.
  Point ToTextPoint(const Point& point);
  Point ToViewPoint(const Point& point);

  // Returns the width of content, which reserves room for the cursor if
  // |cursor_enabled_| is true.
  int GetContentWidth();

  // Returns display offset based on current text alignment.
  Point GetAlignmentOffset();

  // Returns the origin point for drawing text. Does not account for font
  // baseline, as needed by Skia.
  Point GetOriginForDrawing();

  // Applies fade effects to |renderer|.
  void ApplyFadeEffects(internal::SkiaTextRenderer* renderer);

  // Applies text shadows to |renderer|.
  void ApplyTextShadows(internal::SkiaTextRenderer* renderer);

  // A convenience function to check whether the glyph attached to the caret
  // is within the given range.
  static bool RangeContainsCaret(const ui::Range& range,
                                 size_t caret_pos,
                                 LogicalCursorDirection caret_affinity);

 private:
  friend class RenderTextTest;

  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, DefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, CustomDefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ApplyStyleRange);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, StyleRangesAdjust);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, PasswordCensorship);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, GraphemePositions);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, EdgeSelectionModels);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, OriginForDrawing);

  // Set the cursor to |position|, with the caret trailing the previous
  // grapheme, or if there is no previous grapheme, leading the cursor position.
  // If |select| is false, the selection start is moved to the same position.
  // If the |position| is not a cursorable position (not on grapheme boundary),
  // it is a NO-OP.
  void MoveCursorTo(size_t position, bool select);

  // Update the cached bounds and display offset to ensure that the current
  // cursor is within the visible display area.
  void UpdateCachedBoundsAndOffset();

  // Draw the selection and cursor.
  void DrawSelection(Canvas* canvas);
  void DrawCursor(Canvas* canvas);

  // Logical UTF-16 string data to be drawn.
  string16 text_;

  // Horizontal alignment of the text with respect to |display_rect_|.
  HorizontalAlignment horizontal_alignment_;

  // A list of fonts used to render |text_|.
  FontList font_list_;

  // Logical selection range and visual cursor position.
  SelectionModel selection_model_;

  // The cached cursor bounds; get these bounds with GetUpdatedCursorBounds.
  Rect cursor_bounds_;

  // Specifies whether the cursor is enabled. If disabled, no space is reserved
  // for the cursor when positioning text.
  bool cursor_enabled_;

  // The cursor visibility and insert mode.
  bool cursor_visible_;
  bool insert_mode_;

  // The color used for the cursor.
  SkColor cursor_color_;

  // The focus state of the text.
  bool focused_;

  // Composition text range.
  ui::Range composition_range_;

  // List of style ranges. Elements in the list never overlap each other.
  StyleRanges style_ranges_;
  // The default text style.
  StyleRange default_style_;

  // True if this is an obscured (password) field.
  bool obscured_;

  // Fade text head and/or tail, if text doesn't fit into |display_rect_|.
  bool fade_head_;
  bool fade_tail_;

  // Is the background transparent (either partially or fully)?
  bool background_is_transparent_;

  // The local display area for rendering the text.
  Rect display_rect_;

  // The offset for the text to be drawn, relative to the display area.
  // Get this point with GetUpdatedDisplayOffset (or risk using a stale value).
  Point display_offset_;

  // The cached bounds and offset are invalidated by changes to the cursor,
  // selection, font, and other operations that adjust the visible text bounds.
  bool cached_bounds_and_offset_valid_;

  // Text shadows to be drawn.
  std::vector<ShadowValue> text_shadows_;

  DISALLOW_COPY_AND_ASSIGN(RenderText);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_H_
