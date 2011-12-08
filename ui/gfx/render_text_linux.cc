// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_linux.h"

#include <pango/pangocairo.h>
#include <algorithm>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/pango_util.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"

namespace {

// TODO(xji): instead of converting each R or G or B from 8-bit to 16-bit,
// it should also massage A in the conversion.
int ConvertColorFrom8BitTo16Bit(int c) {
  return (c << 8) + c;
}

}  // namespace

// TODO(xji): index saved in upper layer is utf16 index. Pango uses utf8 index.
// Since caret_pos is used internally, we could save utf8 index for caret_pos
// to avoid conversion.

namespace gfx {

RenderTextLinux::RenderTextLinux()
    : layout_(NULL),
      current_line_(NULL),
      log_attrs_(NULL),
      num_log_attrs_(0),
      layout_text_(NULL),
      layout_text_len_(0) {
}

RenderTextLinux::~RenderTextLinux() {
  ResetLayout();
}

RenderText* RenderText::CreateRenderText() {
  return new RenderTextLinux;
}

base::i18n::TextDirection RenderTextLinux::GetTextDirection() {
  EnsureLayout();

  PangoDirection base_dir = pango_find_base_dir(layout_text_, -1);
  if (base_dir == PANGO_DIRECTION_RTL || base_dir == PANGO_DIRECTION_WEAK_RTL)
    return base::i18n::RIGHT_TO_LEFT;
  return base::i18n::LEFT_TO_RIGHT;
}

int RenderTextLinux::GetStringWidth() {
  EnsureLayout();
  int width;
  pango_layout_get_pixel_size(layout_, &width, NULL);
  return width;
}

SelectionModel RenderTextLinux::FindCursorPosition(const Point& point) {
  EnsureLayout();

  if (text().empty())
    return SelectionModel(0, 0, SelectionModel::LEADING);

  Point p(ToTextPoint(point));

  // When the point is outside of text, return HOME/END position.
  if (p.x() < 0)
    return LeftEndSelectionModel();
  else if (p.x() > GetStringWidth())
    return RightEndSelectionModel();

  int caret_pos, trailing;
  pango_layout_xy_to_index(layout_, p.x() * PANGO_SCALE, p.y() * PANGO_SCALE,
                           &caret_pos, &trailing);

  size_t selection_end = caret_pos;
  if (trailing > 0) {
    const char* ch = g_utf8_offset_to_pointer(layout_text_ + caret_pos,
                                              trailing);
    DCHECK_GE(ch, layout_text_);
    DCHECK_LE(ch, layout_text_ + layout_text_len_);
    selection_end = ch - layout_text_;
  }

  return SelectionModel(
      Utf8IndexToUtf16Index(selection_end),
      Utf8IndexToUtf16Index(caret_pos),
      trailing > 0 ? SelectionModel::TRAILING : SelectionModel::LEADING);
}

Rect RenderTextLinux::GetCursorBounds(const SelectionModel& selection,
                                      bool insert_mode) {
  EnsureLayout();

  size_t caret_pos = insert_mode ? selection.caret_pos() :
                                   selection.selection_end();
  PangoRectangle pos;
  pango_layout_index_to_pos(layout_, Utf16IndexToUtf8Index(caret_pos), &pos);

  SelectionModel::CaretPlacement caret_placement = selection.caret_placement();
  int x = pos.x;
  if ((insert_mode && caret_placement == SelectionModel::TRAILING) ||
      (!insert_mode && pos.width < 0))
    x += pos.width;
  x = PANGO_PIXELS(x);

  int h = std::min(display_rect().height(), PANGO_PIXELS(pos.height));
  Rect bounds(x, (display_rect().height() - h) / 2, 0, h);
  bounds.set_origin(ToViewPoint(bounds.origin()));

  if (!insert_mode)
    bounds.set_width(std::abs(pos.width));

  return bounds;
}

SelectionModel RenderTextLinux::GetLeftSelectionModel(
    const SelectionModel& current,
    BreakType break_type) {
  EnsureLayout();

  if (break_type == LINE_BREAK || text().empty())
    return LeftEndSelectionModel();
  if (break_type == CHARACTER_BREAK)
    return LeftSelectionModel(current);
  DCHECK(break_type == WORD_BREAK);
  return LeftSelectionModelByWord(current);
}

SelectionModel RenderTextLinux::GetRightSelectionModel(
    const SelectionModel& current,
    BreakType break_type) {
  EnsureLayout();

  if (break_type == LINE_BREAK || text().empty())
    return RightEndSelectionModel();
  if (break_type == CHARACTER_BREAK)
    return RightSelectionModel(current);
  DCHECK(break_type == WORD_BREAK);
  return RightSelectionModelByWord(current);
}

SelectionModel RenderTextLinux::LeftEndSelectionModel() {
  if (GetTextDirection() == base::i18n::RIGHT_TO_LEFT) {
    if (current_line_->runs) {
      PangoLayoutRun* first_visual_run =
          reinterpret_cast<PangoLayoutRun*>(current_line_->runs->data);
      PangoItem* item = first_visual_run->item;
      if (item->analysis.level % 2 == 0) {  // LTR.
        size_t caret = Utf8IndexToUtf16Index(item->offset);
        return SelectionModel(text().length(), caret, SelectionModel::LEADING);
      } else {  // RTL.
        size_t caret = Utf16IndexOfAdjacentGrapheme(item->offset + item->length,
                                                    false);
        return SelectionModel(text().length(), caret, SelectionModel::TRAILING);
      }
    }
  }
  return SelectionModel(0, 0, SelectionModel::LEADING);
}

SelectionModel RenderTextLinux::RightEndSelectionModel() {
  if (GetTextDirection() == base::i18n::LEFT_TO_RIGHT) {
    PangoLayoutRun* last_visual_run = GetLastRun();
    if (last_visual_run) {
      PangoItem* item = last_visual_run->item;
      if (item->analysis.level % 2 == 0) {  // LTR.
        size_t caret = Utf16IndexOfAdjacentGrapheme(item->offset + item->length,
                                                    false);
        return SelectionModel(text().length(), caret, SelectionModel::TRAILING);
      } else {  // RTL.
        size_t caret = Utf8IndexToUtf16Index(item->offset);
        return SelectionModel(text().length(), caret, SelectionModel::LEADING);
      }
    }
  }
  return SelectionModel(0, 0, SelectionModel::LEADING);
}

void RenderTextLinux::SetSelectionModel(const SelectionModel& model) {
  if (GetSelectionStart() != model.selection_start() ||
      GetCursorPosition() != model.selection_end()) {
    selection_visual_bounds_.clear();
  }

  RenderText::SetSelectionModel(model);
}

void RenderTextLinux::GetSubstringBounds(size_t from,
                                         size_t to,
                                         std::vector<Rect>* bounds) {
  DCHECK(from <= text().length());
  DCHECK(to <= text().length());

  bounds->clear();
  if (from == to)
    return;

  EnsureLayout();

  if (from == GetSelectionStart() && to == GetCursorPosition())
    GetSelectionBounds(bounds);
  else
    CalculateSubstringBounds(from, to, bounds);
}

bool RenderTextLinux::IsCursorablePosition(size_t position) {
  if (position == 0 && text().empty())
    return true;

  EnsureLayout();
  return (position < static_cast<size_t>(num_log_attrs_) &&
          log_attrs_[position].is_cursor_position);
}

void RenderTextLinux::UpdateLayout() {
  ResetLayout();
}

void RenderTextLinux::EnsureLayout() {
  if (layout_ == NULL) {
    CanvasSkia canvas(display_rect().width(), display_rect().height(), false);
    skia::ScopedPlatformPaint scoped_platform_paint(canvas.sk_canvas());
    cairo_t* cr = scoped_platform_paint.GetPlatformSurface();

    layout_ = pango_cairo_create_layout(cr);
    SetupPangoLayout(
        layout_,
        text(),
        default_style().font,
        display_rect().width(),
        base::i18n::GetFirstStrongCharacterDirection(text()),
        CanvasSkia::DefaultCanvasTextAlignment());

    // No width set so that the x-axis position is relative to the start of the
    // text. ToViewPoint and ToTextPoint take care of the position conversion
    // between text space and view spaces.
    pango_layout_set_width(layout_, -1);
    // TODO(xji): If RenderText will be used for displaying purpose, such as
    // label, we will need to remove the single-line-mode setting.
    pango_layout_set_single_paragraph_mode(layout_, true);
    SetupPangoAttributes(layout_);

    current_line_ = pango_layout_get_line_readonly(layout_, 0);
    pango_layout_line_ref(current_line_);

    pango_layout_get_log_attrs(layout_, &log_attrs_, &num_log_attrs_);

    layout_text_ = pango_layout_get_text(layout_);
    layout_text_len_ = strlen(layout_text_);
  }
}

void RenderTextLinux::DrawVisualText(Canvas* canvas) {
  Rect bounds(display_rect());

  // Clip the canvas to the text display area.
  SkCanvas* canvas_skia = canvas->GetSkCanvas();

  skia::ScopedPlatformPaint scoped_platform_paint(canvas_skia);
  cairo_t* cr = scoped_platform_paint.GetPlatformSurface();
  cairo_save(cr);
  cairo_rectangle(cr, bounds.x(), bounds.y(), bounds.width(), bounds.height());
  cairo_clip(cr);

  int text_width, text_height;
  pango_layout_get_pixel_size(layout_, &text_width, &text_height);
  Point offset(ToViewPoint(Point()));
  // Vertically centered.
  int text_y = offset.y() + ((bounds.height() - text_height) / 2);
  // TODO(xji): need to use SkCanvas->drawPosText() for gpu acceleration.
  cairo_move_to(cr, offset.x(), text_y);
  pango_cairo_show_layout(cr, layout_);

  cairo_restore(cr);
}

size_t RenderTextLinux::IndexOfAdjacentGrapheme(size_t index, bool next) {
  EnsureLayout();
  return Utf16IndexOfAdjacentGrapheme(Utf16IndexToUtf8Index(index), next);
}

GSList* RenderTextLinux::GetRunContainingPosition(size_t position) const {
  GSList* run = current_line_->runs;
  while (run) {
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t run_start = Utf8IndexToUtf16Index(item->offset);
    size_t run_end = Utf8IndexToUtf16Index(item->offset + item->length);

    if (position >= run_start && position < run_end)
      return run;
    run = run->next;
  }
  return NULL;
}

size_t RenderTextLinux::Utf8IndexOfAdjacentGrapheme(
    size_t utf8_index_of_current_grapheme,
    bool next) const {
  const char* ch = layout_text_ + utf8_index_of_current_grapheme;
  int char_offset = static_cast<int>(g_utf8_pointer_to_offset(layout_text_,
                                                              ch));
  int start_char_offset = char_offset;
  if (!next) {
    if (char_offset > 0) {
      do {
        --char_offset;
      } while (char_offset > 0 && !log_attrs_[char_offset].is_cursor_position);
    }
  } else {
    if (char_offset < num_log_attrs_ - 1) {
      do {
        ++char_offset;
      } while (char_offset < num_log_attrs_ - 1 &&
               !log_attrs_[char_offset].is_cursor_position);
    }
  }

  ch = g_utf8_offset_to_pointer(ch, char_offset - start_char_offset);
  return static_cast<size_t>(ch - layout_text_);
}

size_t RenderTextLinux::Utf16IndexOfAdjacentGrapheme(
    size_t utf8_index_of_current_grapheme,
    bool next) const {
  size_t utf8_index = Utf8IndexOfAdjacentGrapheme(
      utf8_index_of_current_grapheme, next);
  return Utf8IndexToUtf16Index(utf8_index);
}

SelectionModel RenderTextLinux::FirstSelectionModelInsideRun(
    const PangoItem* item) const {
  size_t caret = Utf8IndexToUtf16Index(item->offset);
  size_t cursor = Utf16IndexOfAdjacentGrapheme(item->offset, true);
  return SelectionModel(cursor, caret, SelectionModel::TRAILING);
}

SelectionModel RenderTextLinux::LastSelectionModelInsideRun(
    const PangoItem* item) const {
  size_t caret = Utf16IndexOfAdjacentGrapheme(item->offset + item->length,
                                              false);
  return SelectionModel(caret, caret, SelectionModel::LEADING);
}

// Assume caret_pos in |current| is n, 'l' represents leading in
// caret_placement and 't' represents trailing in caret_placement. Following
// is the calculation from (caret_pos, caret_placement) in |current| to
// (selection_end, caret_pos, caret_placement) when moving cursor left by
// one grapheme (for simplicity, assume each grapheme is one character).
// If n is in LTR run,
// (n, t) ---> (n, n, l).
// (n, l) ---> (n-1, n-1, l) if n is inside run (not at boundary).
// (n, l) ---> goto across run case if n is at run boundary.
// If n is in RTL run,
// (n, l) --> (n+1, n, t).
// (n, t) --> (n+2, n+1, t) if n is inside run.
// (n, t) --> goto across run case if n is at run boundary.
// If n is at run boundary, get its visually left run,
// If left run is LTR run,
// (n, t) --> (left run's end, left run's end, l).
// If left run is RTL run,
// (n, t) --> (left run's begin + 1, left run's begin, t).
SelectionModel RenderTextLinux::LeftSelectionModel(
    const SelectionModel& selection) {
  size_t caret = selection.caret_pos();
  SelectionModel::CaretPlacement caret_placement = selection.caret_placement();
  GSList* run = GetRunContainingPosition(caret);
  DCHECK(run);

  PangoLayoutRun* layout_run = reinterpret_cast<PangoLayoutRun*>(run->data);
  PangoItem* item = layout_run->item;
  size_t run_start = Utf8IndexToUtf16Index(item->offset);
  size_t run_end = Utf8IndexToUtf16Index(item->offset + item->length);

  if (item->analysis.level % 2 == 0) {  // LTR run.
    if (caret_placement == SelectionModel::TRAILING)
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    else if (caret > run_start) {
      caret = GetIndexOfPreviousGrapheme(caret);
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    }
  } else {  // RTL run.
    if (caret_placement == SelectionModel::LEADING) {
      size_t cursor = GetIndexOfNextGrapheme(caret);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    } else if (selection.selection_end() < run_end) {
      caret = GetIndexOfNextGrapheme(caret);
      size_t cursor = GetIndexOfNextGrapheme(caret);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    }
  }

  // The character is at the begin of its run; advance to the previous visual
  // run.
  PangoLayoutRun* prev_run = GetPreviousRun(layout_run);
  if (!prev_run)
    return LeftEndSelectionModel();

  item = prev_run->item;
  return (item->analysis.level % 2) ? FirstSelectionModelInsideRun(item) :
                                      LastSelectionModelInsideRun(item);
}

// Assume caret_pos in |current| is n, 'l' represents leading in
// caret_placement and 't' represents trailing in caret_placement. Following
// is the calculation from (caret_pos, caret_placement) in |current| to
// (selection_end, caret_pos, caret_placement) when moving cursor right by
// one grapheme (for simplicity, assume each grapheme is one character).
// If n is in LTR run,
// (n, l) ---> (n+1, n, t).
// (n, t) ---> (n+2, n+1, t) if n is inside run (not at boundary).
// (n, t) ---> goto across run case if n is at run boundary.
// If n is in RTL run,
// (n, t) --> (n, n, l).
// (n, l) --> (n-1, n-1, l) if n is inside run.
// (n, l) --> goto across run case if n is at run boundary.
// If n is at run boundary, get its visually right run,
// If right run is LTR run,
// (n, t) --> (right run's begin + 1, right run's begin, t).
// If right run is RTL run,
// (n, t) --> (right run's end, right run's end, l).
SelectionModel RenderTextLinux::RightSelectionModel(
    const SelectionModel& selection) {
  size_t caret = selection.caret_pos();
  SelectionModel::CaretPlacement caret_placement = selection.caret_placement();
  GSList* run = GetRunContainingPosition(caret);
  DCHECK(run);

  PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
  size_t run_start = Utf8IndexToUtf16Index(item->offset);
  size_t run_end = Utf8IndexToUtf16Index(item->offset + item->length);

  if (item->analysis.level % 2 == 0) {  // LTR run.
    if (caret_placement == SelectionModel::LEADING) {
      size_t cursor = GetIndexOfNextGrapheme(caret);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    } else if (selection.selection_end() < run_end) {
      caret = GetIndexOfNextGrapheme(caret);
      size_t cursor = GetIndexOfNextGrapheme(caret);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    }
  } else {  // RTL run.
    if (caret_placement == SelectionModel::TRAILING)
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    else if (caret > run_start) {
      caret = GetIndexOfPreviousGrapheme(caret);
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    }
  }

  // The character is at the end of its run; advance to the next visual run.
  GSList* next_run = run->next;
  if (!next_run)
    return RightEndSelectionModel();

  item = reinterpret_cast<PangoLayoutRun*>(next_run->data)->item;
  return (item->analysis.level % 2) ? LastSelectionModelInsideRun(item) :
                                      FirstSelectionModelInsideRun(item);
}

SelectionModel RenderTextLinux::LeftSelectionModelByWord(
    const SelectionModel& selection) {
  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  SelectionModel left_end = LeftEndSelectionModel();
  SelectionModel left(selection);
  while (!left.Equals(left_end)) {
    left = LeftSelectionModel(left);
    size_t caret = left.caret_pos();
    GSList* run = GetRunContainingPosition(caret);
    DCHECK(run);
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t cursor = left.selection_end();
    if (item->analysis.level % 2 == 0) {  // LTR run.
      if (iter.IsStartOfWord(cursor))
        return left;
    } else {  // RTL run.
      if (iter.IsEndOfWord(cursor))
        return left;
    }
  }

  return left_end;
}

SelectionModel RenderTextLinux::RightSelectionModelByWord(
    const SelectionModel& selection) {
  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  SelectionModel right_end = RightEndSelectionModel();
  SelectionModel right(selection);
  while (!right.Equals(right_end)) {
    right = RightSelectionModel(right);
    size_t caret = right.caret_pos();
    GSList* run = GetRunContainingPosition(caret);
    DCHECK(run);
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t cursor = right.selection_end();
    if (item->analysis.level % 2 == 0) {  // LTR run.
      if (iter.IsEndOfWord(cursor))
        return right;
    } else {  // RTL run.
      if (iter.IsStartOfWord(cursor))
        return right;
    }
  }

  return right_end;
}

void RenderTextLinux::ResetLayout() {
  // set_cached_bounds_and_offset_valid(false) is done in RenderText for every
  // operation that triggers ResetLayout().
  if (layout_) {
    g_object_unref(layout_);
    layout_ = NULL;
  }
  if (current_line_) {
    pango_layout_line_unref(current_line_);
    current_line_ = NULL;
  }
  if (log_attrs_) {
    g_free(log_attrs_);
    log_attrs_ = NULL;
    num_log_attrs_ = 0;
  }
  if (!selection_visual_bounds_.empty())
    selection_visual_bounds_.clear();
  layout_text_ = NULL;
  layout_text_len_ = 0;
}

void RenderTextLinux::SetupPangoAttributes(PangoLayout* layout) {
  PangoAttrList* attrs = pango_attr_list_new();

  StyleRanges ranges_of_style(style_ranges());
  ApplyCompositionAndSelectionStyles(&ranges_of_style);

  PlatformFont* default_platform_font = default_style().font.platform_font();

  PangoAttribute* pango_attr;
  for (StyleRanges::const_iterator i = ranges_of_style.begin();
       i < ranges_of_style.end(); ++i) {
    size_t start = std::min(i->range.start(), text().length());
    size_t end = std::min(i->range.end(), text().length());
    if (start >= end)
      continue;

    const Font& font = i->font;
    // In Pango, different fonts means different runs, and it breaks Arabic
    // shaping acorss run boundaries. So, set font only when it is different
    // from the default faont.
    // TODO(xji): we'll eventually need to split up StyleRange into components
    // (ColorRange, FontRange, etc.) so that we can combine adjacent ranges
    // with the same Fonts (to avoid unnecessarily splitting up runs)
    if (font.platform_font() != default_platform_font) {
      PangoFontDescription* desc = font.GetNativeFont();
      pango_attr = pango_attr_font_desc_new(desc);
      AppendPangoAttribute(start, end, pango_attr, attrs);
      pango_font_description_free(desc);
    }

    SkColor foreground = i->foreground;
    pango_attr = pango_attr_foreground_new(
        ConvertColorFrom8BitTo16Bit(SkColorGetR(foreground)),
        ConvertColorFrom8BitTo16Bit(SkColorGetG(foreground)),
        ConvertColorFrom8BitTo16Bit(SkColorGetB(foreground)));
    AppendPangoAttribute(start, end, pango_attr, attrs);

    if (i->underline) {
      pango_attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
      AppendPangoAttribute(start, end, pango_attr, attrs);
    }

    if (i->strike) {
      pango_attr = pango_attr_strikethrough_new(true);
      AppendPangoAttribute(start, end, pango_attr, attrs);
    }
  }

  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);
}

void RenderTextLinux::AppendPangoAttribute(size_t start,
                                           size_t end,
                                           PangoAttribute* pango_attr,
                                           PangoAttrList* attrs) {
  pango_attr->start_index = Utf16IndexToUtf8Index(start);
  pango_attr->end_index = Utf16IndexToUtf8Index(end);
  pango_attr_list_insert(attrs, pango_attr);
}

// TODO(xji): Keep a vector of runs to avoid using a singly-linked list.
PangoLayoutRun* RenderTextLinux::GetPreviousRun(PangoLayoutRun* run) const {
  GSList* current = current_line_->runs;
  GSList* prev = NULL;
  while (current) {
    if (reinterpret_cast<PangoLayoutRun*>(current->data) == run)
      return prev ? reinterpret_cast<PangoLayoutRun*>(prev->data) : NULL;
    prev = current;
    current = current->next;
  }
  return NULL;
}

PangoLayoutRun* RenderTextLinux::GetLastRun() const {
  GSList* current = current_line_->runs;
  while (current && current->next) {
    current = current->next;
  }
  return current ? reinterpret_cast<PangoLayoutRun*>(current->data) : NULL;
}

size_t RenderTextLinux::Utf16IndexToUtf8Index(size_t index) const {
  int32_t utf8_index = 0;
  UErrorCode ec = U_ZERO_ERROR;
  u_strToUTF8(NULL, 0, &utf8_index, text().data(), index, &ec);
  // Even given a destination buffer as NULL and destination capacity as 0,
  // if the output length is equal to or greater than the capacity, then the
  // UErrorCode is set to U_STRING_NOT_TERMINATED_WARNING or
  // U_BUFFER_OVERFLOW_ERROR respectively.
  // Please refer to
  // http://userguide.icu-project.org/strings#TOC-Using-C-Strings:-NUL-Terminated-vs
  // for detail (search for "Note that" below "Preflighting").
  DCHECK(ec == U_BUFFER_OVERFLOW_ERROR ||
         ec == U_STRING_NOT_TERMINATED_WARNING);
  return utf8_index;
}

size_t RenderTextLinux::Utf8IndexToUtf16Index(size_t index) const {
  int32_t utf16_index = 0;
  UErrorCode ec = U_ZERO_ERROR;
  u_strFromUTF8(NULL, 0, &utf16_index, layout_text_, index, &ec);
  DCHECK(ec == U_BUFFER_OVERFLOW_ERROR ||
         ec == U_STRING_NOT_TERMINATED_WARNING);
  return utf16_index;
}

void RenderTextLinux::CalculateSubstringBounds(size_t from,
                                               size_t to,
                                               std::vector<Rect>* bounds) {
  int* ranges;
  int n_ranges;
  size_t from_in_utf8 = Utf16IndexToUtf8Index(from);
  size_t to_in_utf8 = Utf16IndexToUtf8Index(to);
  pango_layout_line_get_x_ranges(
      current_line_,
      std::min(from_in_utf8, to_in_utf8),
      std::max(from_in_utf8, to_in_utf8),
      &ranges,
      &n_ranges);

  int height;
  pango_layout_get_pixel_size(layout_, NULL, &height);

  int y = (display_rect().height() - height) / 2;

  for (int i = 0; i < n_ranges; ++i) {
    int x = PANGO_PIXELS(ranges[2 * i]);
    int width = PANGO_PIXELS(ranges[2 * i + 1]) - x;
    Rect rect(x, y, width, height);
    rect.set_origin(ToViewPoint(rect.origin()));
    bounds->push_back(rect);
  }
  g_free(ranges);
}

void RenderTextLinux::GetSelectionBounds(std::vector<Rect>* bounds) {
  if (selection_visual_bounds_.empty())
    CalculateSubstringBounds(GetSelectionStart(), GetCursorPosition(),
                             &selection_visual_bounds_);
  *bounds = selection_visual_bounds_;
}

}  // namespace gfx
