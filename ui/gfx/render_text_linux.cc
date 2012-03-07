// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_linux.h"

#include <pango/pangocairo.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/text/utf16_indexing.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/pango_util.h"

namespace gfx {

namespace {

// Returns the preceding element in a GSList (O(n)).
GSList* GSListPrevious(GSList* head, GSList* item) {
  GSList* prev = NULL;
  for (GSList* cur = head; cur != item; cur = cur->next) {
    DCHECK(cur);
    prev = cur;
  }
  return prev;
}

// Returns true if the given visual cursor |direction| is logically forward
// motion in the given Pango |item|.
bool IsForwardMotion(VisualCursorDirection direction, const PangoItem* item) {
  bool rtl = item->analysis.level & 1;
  return rtl == (direction == CURSOR_LEFT);
}

// Checks whether |range| contains |index|. This is not the same as calling
// |range.Contains(ui::Range(index))| - as that would return true when
// |index| == |range.end()|.
bool IndexInRange(const ui::Range& range, size_t index) {
  return index >= range.start() && index < range.end();
}

}  // namespace

// TODO(xji): index saved in upper layer is utf16 index. Pango uses utf8 index.
// Since caret_pos is used internally, we could save utf8 index for caret_pos
// to avoid conversion.

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
    return EdgeSelectionModel(CURSOR_LEFT);
  else if (p.x() > GetStringWidth())
    return EdgeSelectionModel(CURSOR_RIGHT);

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
      LayoutIndexToTextIndex(selection_end),
      LayoutIndexToTextIndex(caret_pos),
      trailing > 0 ? SelectionModel::TRAILING : SelectionModel::LEADING);
}

Rect RenderTextLinux::GetCursorBounds(const SelectionModel& selection,
                                      bool insert_mode) {
  EnsureLayout();

  size_t caret_pos = insert_mode ? selection.caret_pos() :
                                   selection.selection_end();
  PangoRectangle pos;
  pango_layout_index_to_pos(layout_, TextIndexToLayoutIndex(caret_pos), &pos);

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
    bounds.set_width(PANGO_PIXELS(std::abs(pos.width)));

  return bounds;
}

// Assume caret_pos in |current| is n, 'l' represents leading in
// caret_placement and 't' represents trailing in caret_placement. Following
// is the calculation from (caret_pos, caret_placement) in |current| to
// (selection_end, caret_pos, caret_placement) when moving cursor left/right by
// one grapheme (for simplicity, assume each grapheme is one character).
// If n is in LTR (if moving left) or RTL (if moving right) run,
// (n, t) --> (n, n, l).
// (n, l) --> (n-1, n-1, l) if n is inside run (not at boundary).
// (n, l) --> goto across run case if n is at run boundary.
// Otherwise,
// (n, l) --> (n+1, n, t).
// (n, t) --> (n+2, n+1, t) if n is inside run.
// (n, t) --> goto across run case if n is at run boundary.
// If n is at run boundary, get its visually left/right run,
// If left/right run is LTR/RTL run,
// (n, t) --> (left/right run's end, left/right run's end, l).
// Otherwise,
// (n, t) --> (left/right run's begin + 1, left/right run's begin, t).
SelectionModel RenderTextLinux::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  size_t caret = selection.caret_pos();
  SelectionModel::CaretPlacement caret_placement = selection.caret_placement();
  GSList* run = GetRunContainingPosition(caret);
  DCHECK(run);

  PangoLayoutRun* layout_run = reinterpret_cast<PangoLayoutRun*>(run->data);
  PangoItem* item = layout_run->item;
  size_t run_start = LayoutIndexToTextIndex(item->offset);
  size_t run_end = LayoutIndexToTextIndex(item->offset + item->length);
  if (!IsForwardMotion(direction, item)) {
    if (caret_placement == SelectionModel::TRAILING)
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    else if (caret > run_start) {
      caret = IndexOfAdjacentGrapheme(caret, CURSOR_BACKWARD);
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    }
  } else {
    if (caret_placement == SelectionModel::LEADING) {
      size_t cursor = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    } else if (selection.selection_end() < run_end) {
      caret = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
      size_t cursor = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    }
  }

  // The character is at the edge of its run; advance to the adjacent visual
  // run.
  // TODO(xji): Keep a vector of runs to avoid using a singly-linked list.
  GSList* adjacent_run = (direction == CURSOR_RIGHT) ?
      run->next : GSListPrevious(current_line_->runs, run);
  if (!adjacent_run)
    return EdgeSelectionModel(direction);

  item = reinterpret_cast<PangoLayoutRun*>(adjacent_run->data)->item;
  return IsForwardMotion(direction, item) ?
      FirstSelectionModelInsideRun(item) : LastSelectionModelInsideRun(item);
}

SelectionModel RenderTextLinux::AdjacentWordSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  if (is_obscured())
    return EdgeSelectionModel(direction);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  SelectionModel end = EdgeSelectionModel(direction);
  SelectionModel cur(selection);
  while (!cur.Equals(end)) {
    cur = AdjacentCharSelectionModel(cur, direction);
    size_t caret = cur.caret_pos();
    GSList* run = GetRunContainingPosition(caret);
    DCHECK(run);
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t cursor = cur.selection_end();
    if (IsForwardMotion(direction, item) ?
        iter.IsEndOfWord(cursor) : iter.IsStartOfWord(cursor))
      return cur;
  }

  return end;
}

SelectionModel RenderTextLinux::EdgeSelectionModel(
    VisualCursorDirection direction) {
  if (direction == GetVisualDirectionOfLogicalEnd()) {
    // Advance to the logical end of the text.
    GSList* run = current_line_->runs;
    if (direction == CURSOR_RIGHT)
      run = g_slist_last(run);
    if (run) {
      PangoLayoutRun* end_run = reinterpret_cast<PangoLayoutRun*>(run->data);
      PangoItem* item = end_run->item;
      if (IsForwardMotion(direction, item)) {
        size_t caret = IndexOfAdjacentGrapheme(
            LayoutIndexToTextIndex(item->offset + item->length),
            CURSOR_BACKWARD);
        return SelectionModel(text().length(), caret, SelectionModel::TRAILING);
      } else {
        size_t caret = LayoutIndexToTextIndex(item->offset);
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

std::vector<Rect> RenderTextLinux::GetSubstringBounds(size_t from, size_t to) {
  DCHECK(from <= text().length());
  DCHECK(to <= text().length());

  if (from == to)
    return std::vector<Rect>();

  EnsureLayout();

  if (from == GetSelectionStart() && to == GetCursorPosition())
    return GetSelectionBounds();
  else
    return CalculateSubstringBounds(from, to);
}

bool RenderTextLinux::IsCursorablePosition(size_t position) {
  if (position == 0 && text().empty())
    return true;

  EnsureLayout();
  ptrdiff_t offset = ui::UTF16IndexToOffset(text(), 0, position);
  return (offset < num_log_attrs_ && log_attrs_[offset].is_cursor_position);
}

void RenderTextLinux::UpdateLayout() {
  ResetLayout();
}

void RenderTextLinux::EnsureLayout() {
  if (layout_ == NULL) {
    CanvasSkia canvas(display_rect().size(), false);
    skia::ScopedPlatformPaint scoped_platform_paint(canvas.sk_canvas());
    cairo_t* cr = scoped_platform_paint.GetPlatformSurface();

    layout_ = pango_cairo_create_layout(cr);
    SetupPangoLayoutWithFontDescription(
        layout_,
        GetDisplayText(),
        font_list().GetFontDescriptionString(),
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

    // These are used by SetupPangoAttributes.
    layout_text_ = pango_layout_get_text(layout_);
    layout_text_len_ = strlen(layout_text_);

    SetupPangoAttributes(layout_);

    current_line_ = pango_layout_get_line_readonly(layout_, 0);
    pango_layout_line_ref(current_line_);

    pango_layout_get_log_attrs(layout_, &log_attrs_, &num_log_attrs_);
  }
}

void RenderTextLinux::SetupPangoAttributes(PangoLayout* layout) {
  PangoAttrList* attrs = pango_attr_list_new();

  int default_font_style = font_list().GetFontStyle();
  for (StyleRanges::const_iterator i = style_ranges().begin();
       i < style_ranges().end(); ++i) {
    // In Pango, different fonts means different runs, and it breaks Arabic
    // shaping across run boundaries. So, set font only when it is different
    // from the default font.
    // TODO(xji): We'll eventually need to split up StyleRange into components
    // (ColorRange, FontRange, etc.) so that we can combine adjacent ranges
    // with the same Fonts (to avoid unnecessarily splitting up runs).
    if (i->font_style != default_font_style) {
      FontList derived_font_list = font_list().DeriveFontList(i->font_style);
      PangoFontDescription* desc = pango_font_description_from_string(
          derived_font_list.GetFontDescriptionString().c_str());

      PangoAttribute* pango_attr = pango_attr_font_desc_new(desc);
      pango_attr->start_index = TextIndexToLayoutIndex(i->range.start());
      pango_attr->end_index = TextIndexToLayoutIndex(i->range.end());
      pango_attr_list_insert(attrs, pango_attr);
      pango_font_description_free(desc);
    }
  }

  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);
}

void RenderTextLinux::DrawVisualText(Canvas* canvas) {
  DCHECK(layout_);

  Point offset(GetOriginForSkiaDrawing());
  SkScalar x = SkIntToScalar(offset.x());
  SkScalar y = SkIntToScalar(offset.y());

  std::vector<SkPoint> pos;
  std::vector<uint16> glyphs;

  StyleRanges styles(style_ranges());
  ApplyCompositionAndSelectionStyles(&styles);

  // Pre-calculate UTF8 indices from UTF16 indices.
  // TODO(asvitkine): Can we cache these?
  std::vector<ui::Range> style_ranges_utf8;
  style_ranges_utf8.reserve(styles.size());
  size_t start_index = 0;
  for (size_t i = 0; i < styles.size(); ++i) {
    size_t end_index = TextIndexToLayoutIndex(styles[i].range.end());
    style_ranges_utf8.push_back(ui::Range(start_index, end_index));
    start_index = end_index;
  }

  internal::SkiaTextRenderer renderer(canvas);
  ApplyFadeEffects(&renderer);
  renderer.SetFontSmoothingSettings(true, !background_is_transparent());

  for (GSList* it = current_line_->runs; it; it = it->next) {
    PangoLayoutRun* run = reinterpret_cast<PangoLayoutRun*>(it->data);
    int glyph_count = run->glyphs->num_glyphs;
    if (glyph_count == 0)
      continue;

    size_t run_start = run->item->offset;
    size_t first_glyph_byte_index = run_start + run->glyphs->log_clusters[0];
    size_t style_increment = IsForwardMotion(CURSOR_RIGHT, run->item) ? 1 : -1;

    // Find the initial style for this run.
    // TODO(asvitkine): Can we avoid looping here, e.g. by caching this per run?
    int style = -1;
    for (size_t i = 0; i < style_ranges_utf8.size(); ++i) {
      if (IndexInRange(style_ranges_utf8[i], first_glyph_byte_index)) {
        style = i;
        break;
      }
    }
    DCHECK_GE(style, 0);

    PangoFontDescription* native_font =
        pango_font_describe(run->item->analysis.font);

    const char* family_name = pango_font_description_get_family(native_font);
    SkAutoTUnref<SkTypeface> typeface(
        SkTypeface::CreateFromName(family_name, SkTypeface::kNormal));
    renderer.SetTypeface(typeface.get());
    renderer.SetTextSize(GetPangoFontSizeInPixels(native_font));

    pango_font_description_free(native_font);

    SkScalar glyph_x = x;
    SkScalar start_x = x;
    int start = 0;

    glyphs.resize(glyph_count);
    pos.resize(glyph_count);

    for (int i = 0; i < glyph_count; ++i) {
      const PangoGlyphInfo& glyph = run->glyphs->glyphs[i];
      glyphs[i] = static_cast<uint16>(glyph.glyph);
      pos[i].set(glyph_x + PANGO_PIXELS(glyph.geometry.x_offset),
                 y + PANGO_PIXELS(glyph.geometry.y_offset));
      glyph_x += PANGO_PIXELS(glyph.geometry.width);

      // If this glyph is beyond the current style, draw the glyphs so far and
      // advance to the next style.
      size_t glyph_byte_index = run_start + run->glyphs->log_clusters[i];
      DCHECK_GE(style, 0);
      DCHECK_LT(style, static_cast<int>(styles.size()));
      if (!IndexInRange(style_ranges_utf8[style], glyph_byte_index)) {
        // TODO(asvitkine): For cases like "fi", where "fi" is a single glyph
        //                  but can span multiple styles, Pango splits the
        //                  styles evenly over the glyph. We can do this too by
        //                  clipping and drawing the glyph several times.
        renderer.SetForegroundColor(styles[style].foreground);
        renderer.SetFontStyle(styles[style].font_style);
        renderer.DrawPosText(&pos[start], &glyphs[start], i - start);
        renderer.DrawDecorations(start_x, y, glyph_x - start_x, styles[style]);

        start = i;
        start_x = glyph_x;
        // Loop to find the next style, in case the glyph spans multiple styles.
        do {
          style += style_increment;
        } while (style >= 0 && style < static_cast<int>(styles.size()) &&
                 !IndexInRange(style_ranges_utf8[style], glyph_byte_index));
      }
    }

    // Draw the remaining glyphs.
    renderer.SetForegroundColor(styles[style].foreground);
    renderer.SetFontStyle(styles[style].font_style);
    renderer.DrawPosText(&pos[start], &glyphs[start], glyph_count - start);
    renderer.DrawDecorations(start_x, y, glyph_x - start_x, styles[style]);
    x = glyph_x;
  }
}

size_t RenderTextLinux::IndexOfAdjacentGrapheme(
    size_t index,
    LogicalCursorDirection direction) {
  if (index > text().length())
    return text().length();
  EnsureLayout();
  ptrdiff_t char_offset = ui::UTF16IndexToOffset(text(), 0, index);
  if (direction == CURSOR_BACKWARD) {
    if (char_offset > 0) {
      do {
        --char_offset;
      } while (char_offset > 0 && !log_attrs_[char_offset].is_cursor_position);
    }
  } else {  // direction == CURSOR_FORWARD
    if (char_offset < num_log_attrs_ - 1) {
      do {
        ++char_offset;
      } while (char_offset < num_log_attrs_ - 1 &&
               !log_attrs_[char_offset].is_cursor_position);
    }
  }
  return ui::UTF16OffsetToIndex(text(), 0, char_offset);
}

GSList* RenderTextLinux::GetRunContainingPosition(size_t position) const {
  position = TextIndexToLayoutIndex(position);
  for (GSList* run = current_line_->runs; run; run = run->next) {
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    if (position >= static_cast<size_t>(item->offset) &&
        position < static_cast<size_t>(item->offset + item->length))
      return run;
  }
  return NULL;
}

SelectionModel RenderTextLinux::FirstSelectionModelInsideRun(
    const PangoItem* item) {
  size_t caret = LayoutIndexToTextIndex(item->offset);
  size_t cursor = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
  return SelectionModel(cursor, caret, SelectionModel::TRAILING);
}

SelectionModel RenderTextLinux::LastSelectionModelInsideRun(
    const PangoItem* item) {
  size_t caret = IndexOfAdjacentGrapheme(
      LayoutIndexToTextIndex(item->offset + item->length), CURSOR_BACKWARD);
  return SelectionModel(caret, caret, SelectionModel::LEADING);
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

size_t RenderTextLinux::TextIndexToLayoutIndex(size_t text_index) const {
  // If the text is obscured then |layout_text_| is not the same as |text()|,
  // but whether or not the text is obscured, the character (code point) offset
  // in |layout_text_| is the same as that in |text()|.
  DCHECK(layout_);
  ptrdiff_t offset = ui::UTF16IndexToOffset(text(), 0, text_index);
  const char* layout_pointer = g_utf8_offset_to_pointer(layout_text_, offset);
  return (layout_pointer - layout_text_);
}

size_t RenderTextLinux::LayoutIndexToTextIndex(size_t layout_index) const {
  // See |TextIndexToLayoutIndex()|.
  DCHECK(layout_);
  const char* layout_pointer = layout_text_ + layout_index;
  long offset = g_utf8_pointer_to_offset(layout_text_, layout_pointer);
  return ui::UTF16OffsetToIndex(text(), 0, offset);
}

std::vector<Rect> RenderTextLinux::CalculateSubstringBounds(size_t from,
                                                            size_t to) {
  int* ranges;
  int n_ranges;
  size_t from_in_utf8 = TextIndexToLayoutIndex(from);
  size_t to_in_utf8 = TextIndexToLayoutIndex(to);
  pango_layout_line_get_x_ranges(
      current_line_,
      std::min(from_in_utf8, to_in_utf8),
      std::max(from_in_utf8, to_in_utf8),
      &ranges,
      &n_ranges);

  int height;
  pango_layout_get_pixel_size(layout_, NULL, &height);

  int y = (display_rect().height() - height) / 2;

  std::vector<Rect> bounds;
  for (int i = 0; i < n_ranges; ++i) {
    int x = PANGO_PIXELS(ranges[2 * i]);
    int width = PANGO_PIXELS(ranges[2 * i + 1]) - x;
    Rect rect(x, y, width, height);
    rect.set_origin(ToViewPoint(rect.origin()));
    bounds.push_back(rect);
  }
  g_free(ranges);
  return bounds;
}

std::vector<Rect> RenderTextLinux::GetSelectionBounds() {
  if (selection_visual_bounds_.empty())
    selection_visual_bounds_ =
        CalculateSubstringBounds(GetSelectionStart(), GetCursorPosition());
  return selection_visual_bounds_;
}

}  // namespace gfx
