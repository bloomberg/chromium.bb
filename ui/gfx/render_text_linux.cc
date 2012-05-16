// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_linux.h"

#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/text/utf16_indexing.h"
#include "ui/gfx/canvas.h"
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

// Sends an empty query to FontConfig and checks whether subpixel rendering is
// enabled or not in the returned settings.  Caches the result.
bool IsSubpixelRenderingEnabledInFontConfig() {
  static bool subpixel_enabled = false;
  static bool already_queried = false;

  if (already_queried)
    return subpixel_enabled;

  // TODO(derat): Create font_config_util.h/cc and move this there.
  FcPattern* pattern = FcPatternCreate();
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  DCHECK(match);
  int fc_rgba = FC_RGBA_RGB;
  FcPatternGetInteger(match, FC_RGBA, 0, &fc_rgba);
  FcPatternDestroy(pattern);
  FcPatternDestroy(match);

  already_queried = true;
  subpixel_enabled = (fc_rgba != FC_RGBA_NONE);
  return subpixel_enabled;
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

Size RenderTextLinux::GetStringSize() {
  EnsureLayout();
  int width = 0, height = 0;
  pango_layout_get_pixel_size(layout_, &width, &height);
  return Size(width, height);
}

SelectionModel RenderTextLinux::FindCursorPosition(const Point& point) {
  EnsureLayout();

  if (text().empty())
    return SelectionModel(0, CURSOR_FORWARD);

  Point p(ToTextPoint(point));

  // When the point is outside of text, return HOME/END position.
  if (p.x() < 0)
    return EdgeSelectionModel(CURSOR_LEFT);
  else if (p.x() > GetStringSize().width())
    return EdgeSelectionModel(CURSOR_RIGHT);

  int caret_pos = 0, trailing = 0;
  pango_layout_xy_to_index(layout_, p.x() * PANGO_SCALE, p.y() * PANGO_SCALE,
                           &caret_pos, &trailing);

  DCHECK_GE(trailing, 0);
  if (trailing > 0) {
    caret_pos = g_utf8_offset_to_pointer(layout_text_ + caret_pos,
                                         trailing) - layout_text_;
    DCHECK_LE(static_cast<size_t>(caret_pos), layout_text_len_);
  }

  return SelectionModel(LayoutIndexToTextIndex(caret_pos),
                        (trailing > 0) ? CURSOR_BACKWARD : CURSOR_FORWARD);
}

std::vector<RenderText::FontSpan> RenderTextLinux::GetFontSpansForTesting() {
  EnsureLayout();

  std::vector<RenderText::FontSpan> spans;
  for (GSList* it = current_line_->runs; it; it = it->next) {
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(it->data)->item;
    const int start = LayoutIndexToTextIndex(item->offset);
    const int end = LayoutIndexToTextIndex(item->offset + item->length);
    const ui::Range range(start, end);

    ScopedPangoFontDescription desc(pango_font_describe(item->analysis.font));
    spans.push_back(RenderText::FontSpan(Font(desc.get()), range));
  }

  return spans;
}

SelectionModel RenderTextLinux::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  GSList* run = GetRunContainingCaret(selection);
  if (!run) {
    // The cursor is not in any run: we're at the visual and logical edge.
    SelectionModel edge = EdgeSelectionModel(direction);
    if (edge.caret_pos() == selection.caret_pos())
      return edge;
    else
      run = (direction == CURSOR_RIGHT) ?
          current_line_->runs : g_slist_last(current_line_->runs);
  } else {
    // If the cursor is moving within the current run, just move it by one
    // grapheme in the appropriate direction.
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t caret = selection.caret_pos();
    if (IsForwardMotion(direction, item)) {
      if (caret < LayoutIndexToTextIndex(item->offset + item->length)) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
        return SelectionModel(caret, CURSOR_BACKWARD);
      }
    } else {
      if (caret > LayoutIndexToTextIndex(item->offset)) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_BACKWARD);
        return SelectionModel(caret, CURSOR_FORWARD);
      }
    }
    // The cursor is at the edge of a run; move to the visually adjacent run.
    // TODO(xji): Keep a vector of runs to avoid using a singly-linked list.
    run = (direction == CURSOR_RIGHT) ?
        run->next : GSListPrevious(current_line_->runs, run);
    if (!run)
      return EdgeSelectionModel(direction);
  }
  PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
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

  SelectionModel cur(selection);
  for (;;) {
    cur = AdjacentCharSelectionModel(cur, direction);
    GSList* run = GetRunContainingCaret(cur);
    if (!run)
      break;
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t cursor = cur.caret_pos();
    if (IsForwardMotion(direction, item) ?
        iter.IsEndOfWord(cursor) : iter.IsStartOfWord(cursor))
      break;
  }

  return cur;
}

void RenderTextLinux::SetSelectionModel(const SelectionModel& model) {
  if (selection() != model.selection())
    selection_visual_bounds_.clear();

  RenderText::SetSelectionModel(model);
}

void RenderTextLinux::GetGlyphBounds(size_t index,
                                     ui::Range* xspan,
                                     int* height) {
  PangoRectangle pos;
  pango_layout_index_to_pos(layout_, TextIndexToLayoutIndex(index), &pos);
  // TODO(derat): Support fractional ranges for subpixel positioning?
  *xspan = ui::Range(PANGO_PIXELS(pos.x), PANGO_PIXELS(pos.x + pos.width));
  *height = PANGO_PIXELS(pos.height);
}

std::vector<Rect> RenderTextLinux::GetSubstringBounds(ui::Range range) {
  DCHECK_LE(range.GetMax(), text().length());

  if (range.is_empty())
    return std::vector<Rect>();

  EnsureLayout();
  if (range == selection())
    return GetSelectionBounds();
  return CalculateSubstringBounds(range);
}

bool RenderTextLinux::IsCursorablePosition(size_t position) {
  if (position == 0 && text().empty())
    return true;
  if (position >= text().length())
    return position == text().length();
  if (!ui::IsValidCodePointIndex(text(), position))
    return false;

  EnsureLayout();
  ptrdiff_t offset = ui::UTF16IndexToOffset(text(), 0, position);
  return (offset < num_log_attrs_ && log_attrs_[offset].is_cursor_position);
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

void RenderTextLinux::EnsureLayout() {
  if (layout_ == NULL) {
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
    cairo_t* cr = cairo_create(surface);

    layout_ = pango_cairo_create_layout(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    SetupPangoLayoutWithFontDescription(
        layout_,
        GetDisplayText(),
        font_list().GetFontDescriptionString(),
        display_rect().width(),
        base::i18n::GetFirstStrongCharacterDirection(text()),
        Canvas::DefaultCanvasTextAlignment());

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
      ScopedPangoFontDescription desc(pango_font_description_from_string(
          derived_font_list.GetFontDescriptionString().c_str()));

      PangoAttribute* pango_attr = pango_attr_font_desc_new(desc.get());
      pango_attr->start_index = TextIndexToLayoutIndex(i->range.start());
      pango_attr->end_index = TextIndexToLayoutIndex(i->range.end());
      pango_attr_list_insert(attrs, pango_attr);
    }
  }

  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);
}

void RenderTextLinux::DrawVisualText(Canvas* canvas) {
  DCHECK(layout_);

  Point offset(GetOriginForDrawing());
  // Skia will draw glyphs with respect to the baseline.
  offset.Offset(0, PANGO_PIXELS(pango_layout_get_baseline(layout_)));

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
  ApplyTextShadows(&renderer);
  renderer.SetFontSmoothingSettings(
      true /* enable_smoothing */,
      IsSubpixelRenderingEnabledInFontConfig() && !background_is_transparent());

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

    ScopedPangoFontDescription desc(
        pango_font_describe(run->item->analysis.font));

    const std::string family_name =
        pango_font_description_get_family(desc.get());
    renderer.SetTextSize(GetPangoFontSizeInPixels(desc.get()));

    SkScalar glyph_x = x;
    SkScalar start_x = x;
    int start = 0;

    glyphs.resize(glyph_count);
    pos.resize(glyph_count);

    for (int i = 0; i < glyph_count; ++i) {
      const PangoGlyphInfo& glyph = run->glyphs->glyphs[i];
      glyphs[i] = static_cast<uint16>(glyph.glyph);
      // Use pango_units_to_double() rather than PANGO_PIXELS() here so that
      // units won't get rounded to the pixel grid if we're using subpixel
      // positioning.
      pos[i].set(glyph_x + pango_units_to_double(glyph.geometry.x_offset),
                 y + pango_units_to_double(glyph.geometry.y_offset));
      glyph_x += pango_units_to_double(glyph.geometry.width);

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
        renderer.SetFontFamilyWithStyle(family_name, styles[style].font_style);
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
    renderer.SetFontFamilyWithStyle(family_name, styles[style].font_style);
    renderer.DrawPosText(&pos[start], &glyphs[start], glyph_count - start);
    renderer.DrawDecorations(start_x, y, glyph_x - start_x, styles[style]);
    x = glyph_x;
  }
}

GSList* RenderTextLinux::GetRunContainingCaret(
    const SelectionModel& caret) const {
  size_t position = TextIndexToLayoutIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  GSList* run = current_line_->runs;
  while (run) {
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    ui::Range item_range(item->offset, item->offset + item->length);
    if (RangeContainsCaret(item_range, position, affinity))
      return run;
    run = run->next;
  }
  return NULL;
}

SelectionModel RenderTextLinux::FirstSelectionModelInsideRun(
    const PangoItem* item) {
  size_t caret = IndexOfAdjacentGrapheme(
      LayoutIndexToTextIndex(item->offset), CURSOR_FORWARD);
  return SelectionModel(caret, CURSOR_BACKWARD);
}

SelectionModel RenderTextLinux::LastSelectionModelInsideRun(
    const PangoItem* item) {
  size_t caret = IndexOfAdjacentGrapheme(
      LayoutIndexToTextIndex(item->offset + item->length), CURSOR_BACKWARD);
  return SelectionModel(caret, CURSOR_FORWARD);
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

std::vector<Rect> RenderTextLinux::CalculateSubstringBounds(ui::Range range) {
  int* ranges;
  int n_ranges;
  pango_layout_line_get_x_ranges(
      current_line_,
      TextIndexToLayoutIndex(range.GetMin()),
      TextIndexToLayoutIndex(range.GetMax()),
      &ranges,
      &n_ranges);

  int height;
  pango_layout_get_pixel_size(layout_, NULL, &height);

  int y = (display_rect().height() - height) / 2;

  std::vector<Rect> bounds;
  for (int i = 0; i < n_ranges; ++i) {
    // TODO(derat): Support fractional bounds for subpixel positioning?
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
    selection_visual_bounds_ = CalculateSubstringBounds(selection());
  return selection_visual_bounds_;
}

}  // namespace gfx
