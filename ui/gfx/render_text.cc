// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <algorithm>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "unicode/uchar.h"

namespace {

#ifndef NDEBUG
// Check StyleRanges invariant conditions: sorted and non-overlapping ranges.
void CheckStyleRanges(const gfx::StyleRanges& style_ranges, size_t length) {
  if (length == 0) {
    DCHECK(style_ranges.empty()) << "Style ranges exist for empty text.";
    return;
  }
  for (gfx::StyleRanges::size_type i = 0; i < style_ranges.size() - 1; i++) {
    const ui::Range& former = style_ranges[i].range;
    const ui::Range& latter = style_ranges[i + 1].range;
    DCHECK(!former.is_empty()) << "Empty range at " << i << ":" << former;
    DCHECK(former.IsValid()) << "Invalid range at " << i << ":" << former;
    DCHECK(!former.is_reversed()) << "Reversed range at " << i << ":" << former;
    DCHECK(former.end() == latter.start()) << "Ranges gap/overlap/unsorted." <<
        "former:" << former << ", latter:" << latter;
  }
  const gfx::StyleRange& end_style = *style_ranges.rbegin();
  DCHECK(!end_style.range.is_empty()) << "Empty range at end.";
  DCHECK(end_style.range.IsValid()) << "Invalid range at end.";
  DCHECK(!end_style.range.is_reversed()) << "Reversed range at end.";
  DCHECK(end_style.range.end() == length) << "Style and text length mismatch.";
}
#endif

void ApplyStyleRangeImpl(gfx::StyleRanges* style_ranges,
                         gfx::StyleRange style_range) {
  const ui::Range& new_range = style_range.range;
  // Follow StyleRanges invariant conditions: sorted and non-overlapping ranges.
  gfx::StyleRanges::iterator i;
  for (i = style_ranges->begin(); i != style_ranges->end();) {
    if (i->range.end() < new_range.start()) {
      i++;
    } else if (i->range.start() == new_range.end()) {
      break;
    } else if (new_range.Contains(i->range)) {
      i = style_ranges->erase(i);
      if (i == style_ranges->end())
        break;
    } else if (i->range.start() < new_range.start() &&
               i->range.end() > new_range.end()) {
      // Split the current style into two styles.
      gfx::StyleRange split_style = gfx::StyleRange(*i);
      split_style.range.set_end(new_range.start());
      i = style_ranges->insert(i, split_style) + 1;
      i->range.set_start(new_range.end());
      break;
    } else if (i->range.start() < new_range.start()) {
      i->range.set_end(new_range.start());
      i++;
    } else if (i->range.end() > new_range.end()) {
      i->range.set_start(new_range.end());
      break;
    } else {
      NOTREACHED();
    }
  }
  // Add the new range in its sorted location.
  style_ranges->insert(i, style_range);
}

}  // namespace

namespace gfx {

StyleRange::StyleRange()
    : font(),
      foreground(SK_ColorBLACK),
      strike(false),
      underline(false),
      range() {
}

RenderText::~RenderText() {
}

void RenderText::SetText(const string16& text) {
  DCHECK(!composition_range_.IsValid());
  size_t old_text_length = text_.length();
  text_ = text;

  // Update the style ranges as needed.
  if (text_.empty()) {
    style_ranges_.clear();
  } else if (style_ranges_.empty()) {
    ApplyDefaultStyle();
  } else if (text_.length() > old_text_length) {
    style_ranges_.back().range.set_end(text_.length());
  } else if (text_.length() < old_text_length) {
    StyleRanges::iterator i;
    for (i = style_ranges_.begin(); i != style_ranges_.end(); i++) {
      if (i->range.start() >= text_.length()) {
        // Style ranges are sorted and non-overlapping, so all the subsequent
        // style ranges should be out of text_.length() as well.
        style_ranges_.erase(i, style_ranges_.end());
        break;
      }
    }
    // Since style ranges are sorted and non-overlapping, if there is a style
    // range ends beyond text_.length, it must be the last one.
    style_ranges_.back().range.set_end(text_.length());
  }
#ifndef NDEBUG
  CheckStyleRanges(style_ranges_, text_.length());
#endif
  cached_bounds_and_offset_valid_ = false;

  // Reset selection model. SetText should always followed by SetSelectionModel
  // or SetCursorPosition in upper layer.
  SetSelectionModel(SelectionModel(0, 0, SelectionModel::LEADING));

  UpdateLayout();
}

void RenderText::ToggleInsertMode() {
  insert_mode_ = !insert_mode_;
  cached_bounds_and_offset_valid_ = false;
}

void RenderText::SetDisplayRect(const Rect& r) {
  display_rect_ = r;
  cached_bounds_and_offset_valid_ = false;
  UpdateLayout();
}

size_t RenderText::GetCursorPosition() const {
  return selection_model_.selection_end();
}

void RenderText::SetCursorPosition(size_t position) {
  MoveCursorTo(position, false);
}

void RenderText::MoveCursorLeft(BreakType break_type, bool select) {
  SelectionModel position(selection_model());
  position.set_selection_start(GetCursorPosition());
  // Cancelling a selection moves to the edge of the selection.
  if (break_type != LINE_BREAK && !EmptySelection() && !select) {
    // Use the selection start if it is left of the selection end.
    SelectionModel selection_start = GetSelectionModelForSelectionStart();
    if (GetCursorBounds(selection_start, true).x() <
        GetCursorBounds(position, true).x())
      position = selection_start;
    // For word breaks, use the nearest word boundary left of the selection.
    if (break_type == WORD_BREAK)
      position = GetLeftSelectionModel(position, break_type);
  } else {
    position = GetLeftSelectionModel(position, break_type);
  }
  if (select)
    position.set_selection_start(GetSelectionStart());
  MoveCursorTo(position);
}

void RenderText::MoveCursorRight(BreakType break_type, bool select) {
  SelectionModel position(selection_model());
  position.set_selection_start(GetCursorPosition());
  // Cancelling a selection moves to the edge of the selection.
  if (break_type != LINE_BREAK && !EmptySelection() && !select) {
    // Use the selection start if it is right of the selection end.
    SelectionModel selection_start = GetSelectionModelForSelectionStart();
    if (GetCursorBounds(selection_start, true).x() >
        GetCursorBounds(position, true).x())
      position = selection_start;
    // For word breaks, use the nearest word boundary right of the selection.
    if (break_type == WORD_BREAK)
      position = GetRightSelectionModel(position, break_type);
  } else {
    position = GetRightSelectionModel(position, break_type);
  }
  if (select)
    position.set_selection_start(GetSelectionStart());
  MoveCursorTo(position);
}

bool RenderText::MoveCursorTo(const SelectionModel& model) {
  SelectionModel sel(model);
  size_t text_length = text().length();
  // Enforce valid selection model components.
  if (sel.selection_start() > text_length)
    sel.set_selection_start(text_length);
  if (sel.selection_end() > text_length)
    sel.set_selection_end(text_length);
  // The current model only supports caret positions at valid character indices.
  if (text_length == 0) {
    sel.set_caret_pos(0);
    sel.set_caret_placement(SelectionModel::LEADING);
  } else if (sel.caret_pos() >= text_length) {
    SelectionModel end = GetTextDirection() == base::i18n::RIGHT_TO_LEFT ?
        LeftEndSelectionModel() : RightEndSelectionModel();
    sel.set_caret_pos(end.caret_pos());
    sel.set_caret_placement(end.caret_placement());
  }

  if (!IsCursorablePosition(sel.selection_start()) ||
      !IsCursorablePosition(sel.selection_end()) ||
      !IsCursorablePosition(sel.caret_pos()))
    return false;

  bool changed = !sel.Equals(selection_model_);
  SetSelectionModel(sel);
  return changed;
}

bool RenderText::MoveCursorTo(const Point& point, bool select) {
  SelectionModel selection = FindCursorPosition(point);
  if (select)
    selection.set_selection_start(GetSelectionStart());
  return MoveCursorTo(selection);
}

bool RenderText::SelectRange(const ui::Range& range) {
  size_t text_length = text().length();
  size_t start = std::min(range.start(), text_length);
  size_t end = std::min(range.end(), text_length);

  if (!IsCursorablePosition(start) || !IsCursorablePosition(end))
    return false;

  size_t pos = end;
  SelectionModel::CaretPlacement placement = SelectionModel::LEADING;
  if (start < end) {
    pos = GetIndexOfPreviousGrapheme(end);
    DCHECK_LT(pos, end);
    placement = SelectionModel::TRAILING;
  } else if (end == text_length) {
    SelectionModel boundary = GetTextDirection() == base::i18n::RIGHT_TO_LEFT ?
        LeftEndSelectionModel() : RightEndSelectionModel();
    pos = boundary.caret_pos();
    placement = boundary.caret_placement();
  }
  SetSelectionModel(SelectionModel(start, end, pos, placement));
  return true;
}

bool RenderText::IsPointInSelection(const Point& point) {
  if (EmptySelection())
    return false;
  // TODO(xji): should this check whether the point is inside the visual
  // selection bounds? In case of "abcFED", if "ED" is selected, |point| points
  // to the right half of 'c', is the point in selection?
  size_t pos = FindCursorPosition(point).selection_end();
  return (pos >= MinOfSelection() && pos < MaxOfSelection());
}

void RenderText::ClearSelection() {
  SelectionModel sel(selection_model());
  sel.set_selection_start(GetCursorPosition());
  SetSelectionModel(sel);
}

void RenderText::SelectAll() {
  SelectionModel sel(RightEndSelectionModel());
  sel.set_selection_start(LeftEndSelectionModel().selection_start());
  SetSelectionModel(sel);
}

void RenderText::SelectWord() {
  size_t cursor_position = GetCursorPosition();

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return;

  size_t selection_start = cursor_position;
  for (; selection_start != 0; --selection_start) {
    if (iter.IsStartOfWord(selection_start) ||
        iter.IsEndOfWord(selection_start))
      break;
  }

  if (selection_start == cursor_position)
    ++cursor_position;

  for (; cursor_position < text().length(); ++cursor_position) {
    if (iter.IsEndOfWord(cursor_position) ||
        iter.IsStartOfWord(cursor_position))
      break;
  }

  MoveCursorTo(selection_start, false);
  MoveCursorTo(cursor_position, true);
}

const ui::Range& RenderText::GetCompositionRange() const {
  return composition_range_;
}

void RenderText::SetCompositionRange(const ui::Range& composition_range) {
  CHECK(!composition_range.IsValid() ||
        ui::Range(0, text_.length()).Contains(composition_range));
  composition_range_.set_end(composition_range.end());
  composition_range_.set_start(composition_range.start());
  UpdateLayout();
}

void RenderText::ApplyStyleRange(StyleRange style_range) {
  const ui::Range& new_range = style_range.range;
  if (!new_range.IsValid() || new_range.is_empty())
    return;
  CHECK(!new_range.is_reversed());
  CHECK(ui::Range(0, text_.length()).Contains(new_range));
  ApplyStyleRangeImpl(&style_ranges_, style_range);
#ifndef NDEBUG
  CheckStyleRanges(style_ranges_, text_.length());
#endif
  // TODO(xji): only invalidate if font or underline changes.
  cached_bounds_and_offset_valid_ = false;
  UpdateLayout();
}

void RenderText::ApplyDefaultStyle() {
  style_ranges_.clear();
  StyleRange style = StyleRange(default_style_);
  style.range.set_end(text_.length());
  style_ranges_.push_back(style);
  cached_bounds_and_offset_valid_ = false;
  UpdateLayout();
}

base::i18n::TextDirection RenderText::GetTextDirection() {
  if (base::i18n::IsRTL())
    return base::i18n::RIGHT_TO_LEFT;
  return base::i18n::LEFT_TO_RIGHT;
}

int RenderText::GetStringWidth() {
  return default_style_.font.GetStringWidth(text());
}

void RenderText::Draw(Canvas* canvas) {
  EnsureLayout();

  if (!text().empty()) {
    DrawSelection(canvas);
    DrawVisualText(canvas);
  }
  DrawCursor(canvas);
}

SelectionModel RenderText::FindCursorPosition(const Point& point) {
  const Font& font = default_style_.font;
  int left = 0;
  int left_pos = 0;
  int right = font.GetStringWidth(text());
  int right_pos = text().length();

  int x = point.x() - (display_rect_.x() + GetUpdatedDisplayOffset().x());
  if (x <= left) return SelectionModel(left_pos);
  if (x >= right) return SelectionModel(right_pos);
  // binary searching the cursor position.
  // TODO(oshima): use the center of character instead of edge.
  // Binary search may not work for language like Arabic.
  while (std::abs(right_pos - left_pos) > 1) {
    int pivot_pos = left_pos + (right_pos - left_pos) / 2;
    int pivot = font.GetStringWidth(text().substr(0, pivot_pos));
    if (pivot < x) {
      left = pivot;
      left_pos = pivot_pos;
    } else if (pivot == x) {
      return SelectionModel(pivot_pos);
    } else {
      right = pivot;
      right_pos = pivot_pos;
    }
  }
  return SelectionModel(left_pos);
}

const Rect& RenderText::GetUpdatedCursorBounds() {
  UpdateCachedBoundsAndOffset();
  return cursor_bounds_;
}

size_t RenderText::GetIndexOfNextGrapheme(size_t position) {
  return IndexOfAdjacentGrapheme(position, true);
}

SelectionModel RenderText::GetSelectionModelForSelectionStart() {
  size_t selection_start = GetSelectionStart();
  size_t selection_end = GetCursorPosition();
  if (selection_start < selection_end)
    return SelectionModel(selection_start,
                          selection_start,
                          SelectionModel::LEADING);
  else if (selection_start > selection_end)
    return SelectionModel(selection_start,
                          GetIndexOfPreviousGrapheme(selection_start),
                          SelectionModel::TRAILING);
  return selection_model_;
}

RenderText::RenderText()
    : text_(),
      selection_model_(),
      cursor_bounds_(),
      cursor_visible_(false),
      insert_mode_(true),
      composition_range_(ui::Range::InvalidRange()),
      style_ranges_(),
      default_style_(),
      display_rect_(),
      display_offset_(),
      cached_bounds_and_offset_valid_(false) {
}

const Point& RenderText::GetUpdatedDisplayOffset() {
  UpdateCachedBoundsAndOffset();
  return display_offset_;
}

SelectionModel RenderText::GetLeftSelectionModel(const SelectionModel& current,
                                                 BreakType break_type) {
  if (break_type == LINE_BREAK)
    return LeftEndSelectionModel();
  size_t pos = std::max<int>(current.selection_end() - 1, 0);
  if (break_type == CHARACTER_BREAK)
    return SelectionModel(pos, pos, SelectionModel::LEADING);

  // Notes: We always iterate words from the beginning.
  // This is probably fast enough for our usage, but we may
  // want to modify WordIterator so that it can start from the
  // middle of string and advance backwards.
  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return current;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      size_t begin = iter.pos() - iter.GetString().length();
      if (begin == current.selection_end()) {
        // The cursor is at the beginning of a word.
        // Move to previous word.
        break;
      } else if (iter.pos() >= current.selection_end()) {
        // The cursor is in the middle or at the end of a word.
        // Move to the top of current word.
        pos = begin;
        break;
      } else {
        pos = iter.pos() - iter.GetString().length();
      }
    }
  }

  return SelectionModel(pos, pos, SelectionModel::LEADING);
}

SelectionModel RenderText::GetRightSelectionModel(const SelectionModel& current,
                                                  BreakType break_type) {
  if (text_.empty())
    return SelectionModel(0, 0, SelectionModel::LEADING);
  if (break_type == LINE_BREAK)
    return RightEndSelectionModel();
  size_t pos = std::min(current.selection_end() + 1, text().length());
  if (break_type == CHARACTER_BREAK)
    return SelectionModel(pos, pos, SelectionModel::LEADING);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return current;
  while (iter.Advance()) {
    pos = iter.pos();
    if (iter.IsWord() && pos > current.selection_end())
      break;
  }
  return SelectionModel(pos, pos, SelectionModel::LEADING);
}

SelectionModel RenderText::LeftEndSelectionModel() {
  return SelectionModel(0, 0, SelectionModel::LEADING);
}

SelectionModel RenderText::RightEndSelectionModel() {
  size_t cursor = text().length();
  size_t caret_pos = GetIndexOfPreviousGrapheme(cursor);
  SelectionModel::CaretPlacement placement = (caret_pos == cursor) ?
      SelectionModel::LEADING : SelectionModel::TRAILING;
  return SelectionModel(cursor, caret_pos, placement);
}

void RenderText::SetSelectionModel(const SelectionModel& model) {
  DCHECK_LE(model.selection_start(), text().length());
  selection_model_.set_selection_start(model.selection_start());
  DCHECK_LE(model.selection_end(), text().length());
  selection_model_.set_selection_end(model.selection_end());
  DCHECK_LT(model.caret_pos(), std::max<size_t>(text().length(), 1));
  selection_model_.set_caret_pos(model.caret_pos());
  selection_model_.set_caret_placement(model.caret_placement());

  cached_bounds_and_offset_valid_ = false;
  UpdateLayout();
}

size_t RenderText::GetIndexOfPreviousGrapheme(size_t position) {
  return IndexOfAdjacentGrapheme(position, false);
}

void RenderText::ApplyCompositionAndSelectionStyles(
    StyleRanges* style_ranges) const {
  // TODO(msw): This pattern ought to be reconsidered; what about composition
  //            and selection overlaps, retain existing local style features?
  // Apply a composition style override to a copy of the style ranges.
  if (composition_range_.IsValid() && !composition_range_.is_empty()) {
    StyleRange composition_style(default_style_);
    composition_style.underline = true;
    composition_style.range.set_start(composition_range_.start());
    composition_style.range.set_end(composition_range_.end());
    ApplyStyleRangeImpl(style_ranges, composition_style);
  }
  // Apply a selection style override to a copy of the style ranges.
  if (!EmptySelection()) {
    StyleRange selection_style(default_style_);
    selection_style.foreground = kSelectedTextColor;
    selection_style.range.set_start(MinOfSelection());
    selection_style.range.set_end(MaxOfSelection());
    ApplyStyleRangeImpl(style_ranges, selection_style);
  }
}

Point RenderText::ToTextPoint(const Point& point) {
  Point p(point.Subtract(display_rect().origin()));
  p = p.Subtract(GetUpdatedDisplayOffset());
  if (base::i18n::IsRTL())
    p.Offset(GetStringWidth() - display_rect().width() + 1, 0);
  return p;
}

Point RenderText::ToViewPoint(const Point& point) {
  Point p(point.Add(display_rect().origin()));
  p = p.Add(GetUpdatedDisplayOffset());
  if (base::i18n::IsRTL())
    p.Offset(display_rect().width() - GetStringWidth() - 1, 0);
  return p;
}

void RenderText::MoveCursorTo(size_t position, bool select) {
  size_t cursor = std::min(position, text().length());
  size_t caret_pos = GetIndexOfPreviousGrapheme(cursor);
  SelectionModel::CaretPlacement placement = (caret_pos == cursor) ?
      SelectionModel::LEADING : SelectionModel::TRAILING;
  size_t selection_start = select ? GetSelectionStart() : cursor;
  if (IsCursorablePosition(cursor)) {
    SelectionModel sel(selection_start, cursor, caret_pos, placement);
    SetSelectionModel(sel);
  }
}

void RenderText::UpdateCachedBoundsAndOffset() {
  if (cached_bounds_and_offset_valid_)
    return;
  // First, set the valid flag true to calculate the current cursor bounds using
  // the stale |display_offset_|. Applying |delta_offset| at the end of this
  // function will set |cursor_bounds_| and |display_offset_| to correct values.
  cached_bounds_and_offset_valid_ = true;
  cursor_bounds_ = GetCursorBounds(selection_model_, insert_mode_);
  // Update |display_offset_| to ensure the current cursor is visible.
  int display_width = display_rect_.width();
  int string_width = GetStringWidth();
  int delta_offset = 0;
  if (string_width < display_width) {
    // Show all text whenever the text fits to the size.
    delta_offset = -display_offset_.x();
  } else if (cursor_bounds_.right() >= display_rect_.right()) {
    // TODO(xji): when the character overflow is a RTL character, currently, if
    // we pan cursor at the rightmost position, the entered RTL character is not
    // displayed. Should pan cursor to show the last logical characters.
    //
    // Pan to show the cursor when it overflows to the right,
    delta_offset = display_rect_.right() - cursor_bounds_.right() - 1;
  } else if (cursor_bounds_.x() < display_rect_.x()) {
    // TODO(xji): have similar problem as above when overflow character is a
    // LTR character.
    //
    // Pan to show the cursor when it overflows to the left.
    delta_offset = display_rect_.x() - cursor_bounds_.x();
  }
  display_offset_.Offset(delta_offset, 0);
  cursor_bounds_.Offset(delta_offset, 0);
}

void RenderText::DrawSelection(Canvas* canvas) {
  std::vector<Rect> sel;
  GetSubstringBounds(GetSelectionStart(), GetCursorPosition(), &sel);
  SkColor color = focused() ? kFocusedSelectionColor : kUnfocusedSelectionColor;
  for (std::vector<Rect>::const_iterator i = sel.begin(); i < sel.end(); ++i)
    canvas->FillRect(color, *i);
}

void RenderText::DrawCursor(Canvas* canvas) {
  // Paint cursor. Replace cursor is drawn as rectangle for now.
  // TODO(msw): Draw a better cursor with a better indication of association.
  if (cursor_visible() && focused()) {
    Rect r(GetUpdatedCursorBounds());
    canvas->DrawRectInt(kCursorColor, r.x(), r.y(), r.width(), r.height());
  }
}

}  // namespace gfx
