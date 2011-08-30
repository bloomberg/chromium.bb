// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_win.h"

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace {

// The maximum supported number of Uniscribe runs; a SCRIPT_ITEM is 8 bytes.
// TODO(msw): Review memory use/failure? Max string length? Alternate approach?
const int kGuessItems = 100;
const int kMaxItems = 10000;

// The maximum supported number of Uniscribe glyphs; a glyph is 1 word.
// TODO(msw): Review memory use/failure? Max string length? Alternate approach?
const int kMaxGlyphs = 100000;

// TODO(msw): Solve gfx/Uniscribe/Skia text size unit conversion issues.
const float kSkiaFontScale = 1.375;

}  // namespace

namespace gfx {

namespace internal {

TextRun::TextRun()
  : strike(false),
    width(0),
    preceding_run_widths(0),
    glyph_count(0) {
}

}  // namespace internal

RenderTextWin::RenderTextWin()
    : RenderText(),
      script_control_(),
      script_state_(),
      script_cache_(NULL),
      string_width_(0) {
  // Omitting default constructors for script_* would leave POD uninitialized.
  HRESULT hr = 0;

  // TODO(msw): Call ScriptRecordDigitSubstitution on WM_SETTINGCHANGE message.
  // TODO(msw): Use Chrome/profile locale/language settings?
  hr = ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &digit_substitute_);
  DCHECK(SUCCEEDED(hr));

  hr = ScriptApplyDigitSubstitution(&digit_substitute_,
                                    &script_control_,
                                    &script_state_);
  DCHECK(SUCCEEDED(hr));
  script_control_.fMergeNeutralItems = true;

  MoveCursorTo(LeftEndSelectionModel());
}

RenderTextWin::~RenderTextWin() {
  ScriptFreeCache(&script_cache_);
  STLDeleteContainerPointers(runs_.begin(), runs_.end());
}

void RenderTextWin::SetText(const string16& text) {
  // TODO(msw): Skip complex processing if ScriptIsComplex returns false.
  RenderText::SetText(text);
  ItemizeLogicalText();
  LayoutVisualText(CreateCompatibleDC(NULL));
}

void RenderTextWin::SetDisplayRect(const Rect& r) {
  RenderText::SetDisplayRect(r);
  ItemizeLogicalText();
  LayoutVisualText(CreateCompatibleDC(NULL));
}

void RenderTextWin::ApplyStyleRange(StyleRange style_range) {
  RenderText::ApplyStyleRange(style_range);
  ItemizeLogicalText();
  LayoutVisualText(CreateCompatibleDC(NULL));
}

void RenderTextWin::ApplyDefaultStyle() {
  RenderText::ApplyDefaultStyle();
  ItemizeLogicalText();
  LayoutVisualText(CreateCompatibleDC(NULL));
}

int RenderTextWin::GetStringWidth() {
  return string_width_;
}

void RenderTextWin::Draw(Canvas* canvas) {
  skia::ScopedPlatformPaint scoped_platform_paint(canvas->AsCanvasSkia());
  HDC hdc = scoped_platform_paint.GetPlatformSurface();
  int saved_dc = SaveDC(hdc);
  DrawSelection(canvas);
  DrawVisualText(canvas);
  DrawCursor(canvas);
  RestoreDC(hdc, saved_dc);
}

SelectionModel RenderTextWin::FindCursorPosition(const Point& point) {
  if (text().empty())
    return SelectionModel();

  // Find the run that contains the point and adjust the argument location.
  Point p(ToTextPoint(point));
  size_t run_index = GetRunContainingPoint(p);
  if (run_index == runs_.size())
    return (p.x() < 0) ? LeftEndSelectionModel() : RightEndSelectionModel();
  internal::TextRun* run = runs_[run_index];

  int position = 0, trailing = 0;
  HRESULT hr = ScriptXtoCP(p.x() - run->preceding_run_widths,
                           run->range.length(),
                           run->glyph_count,
                           run->logical_clusters.get(),
                           run->visible_attributes.get(),
                           run->advance_widths.get(),
                           &(run->script_analysis),
                           &position,
                           &trailing);
  DCHECK(SUCCEEDED(hr));
  position += run->range.start();

  size_t cursor = position + trailing;
  DCHECK_GE(cursor, 0U);
  DCHECK_LE(cursor, text().length());
  return SelectionModel(cursor, position,
      (trailing > 0) ? SelectionModel::TRAILING : SelectionModel::LEADING);
}

Rect RenderTextWin::GetCursorBounds(const SelectionModel& selection,
                                    bool insert_mode) {
  // Highlight the logical cursor (selection end) when not in insert mode.
  size_t pos = insert_mode ? selection.caret_pos() : selection.selection_end();
  size_t run_index = GetRunContainingPosition(pos);
  internal::TextRun* run = run_index == runs_.size() ? NULL : runs_[run_index];

  int start_x = 0, end_x = 0;
  if (run) {
    HRESULT hr = 0;
    hr = ScriptCPtoX(pos - run->range.start(),
                     false,
                     run->range.length(),
                     run->glyph_count,
                     run->logical_clusters.get(),
                     run->visible_attributes.get(),
                     run->advance_widths.get(),
                     &(run->script_analysis),
                     &start_x);
    DCHECK(SUCCEEDED(hr));
    hr = ScriptCPtoX(pos - run->range.start(),
                     true,
                     run->range.length(),
                     run->glyph_count,
                     run->logical_clusters.get(),
                     run->visible_attributes.get(),
                     run->advance_widths.get(),
                     &(run->script_analysis),
                     &end_x);
    DCHECK(SUCCEEDED(hr));
  }
  // TODO(msw): Use the last visual run's font instead of the default font?
  int height = run ? run->font.GetHeight() : default_style().font.GetHeight();
  Rect rect(std::min(start_x, end_x), 0, std::abs(end_x - start_x), height);
  // Offset to the run start or the right/left end for an out of bounds index.
  // Also center the rect vertically in the display area.
  int text_end_offset = base::i18n::IsRTL() ? 0 : GetStringWidth();
  rect.Offset((run ? run->preceding_run_widths : text_end_offset),
              (display_rect().height() - rect.height()) / 2);
  // Adjust for leading/trailing in insert mode.
  if (insert_mode && run) {
    bool leading = selection.caret_placement() == SelectionModel::LEADING;
    // Adjust the x value for right-side placement.
    if (run->script_analysis.fRTL == leading)
      rect.set_x(rect.right());
    rect.set_width(0);
  }
  rect.set_origin(ToViewPoint(rect.origin()));
  return rect;
}

SelectionModel RenderTextWin::GetLeftSelectionModel(
    const SelectionModel& selection,
    BreakType break_type) {
  if (break_type == LINE_BREAK || text().empty())
    return LeftEndSelectionModel();
  if (break_type == CHARACTER_BREAK)
    return LeftSelectionModel(selection);
  // TODO(msw): Implement word breaking.
  return RenderText::GetLeftSelectionModel(selection, break_type);
}

SelectionModel RenderTextWin::GetRightSelectionModel(
    const SelectionModel& selection,
    BreakType break_type) {
  if (break_type == LINE_BREAK || text().empty())
    return RightEndSelectionModel();
  if (break_type == CHARACTER_BREAK)
    return RightSelectionModel(selection);
  // TODO(msw): Implement word breaking.
  return RenderText::GetRightSelectionModel(selection, break_type);
}

SelectionModel RenderTextWin::LeftEndSelectionModel() {
  if (text().empty())
    return SelectionModel(0, 0, SelectionModel::LEADING);
  size_t cursor = base::i18n::IsRTL() ? text().length() : 0;
  internal::TextRun* run = runs_[visual_to_logical_[0]];
  bool rtl = run->script_analysis.fRTL;
  size_t caret = rtl ? run->range.end() - 1 : run->range.start();
  SelectionModel::CaretPlacement placement =
      rtl ? SelectionModel::TRAILING : SelectionModel::LEADING;
  return SelectionModel(cursor, caret, placement);
}

SelectionModel RenderTextWin::RightEndSelectionModel() {
  if (text().empty())
    return SelectionModel(0, 0, SelectionModel::LEADING);
  size_t cursor = base::i18n::IsRTL() ? 0 : text().length();
  internal::TextRun* run = runs_[visual_to_logical_[runs_.size() - 1]];
  bool rtl = run->script_analysis.fRTL;
  size_t caret = rtl ? run->range.start() : run->range.end() - 1;
  SelectionModel::CaretPlacement placement =
      rtl ? SelectionModel::LEADING : SelectionModel::TRAILING;
  return SelectionModel(cursor, caret, placement);
}

size_t RenderTextWin::GetIndexOfPreviousGrapheme(size_t position) {
  return IndexOfAdjacentGrapheme(position, false);
}

std::vector<Rect> RenderTextWin::GetSubstringBounds(size_t from, size_t to) {
  ui::Range range(from, to);
  DCHECK(ui::Range(0, text().length()).Contains(range));
  Point display_offset(GetUpdatedDisplayOffset());
  std::vector<Rect> bounds;
  HRESULT hr = 0;

  // Add a Rect for each run/selection intersection.
  // TODO(msw): The bounds should probably not always be leading the range ends.
  for (size_t i = 0; i < runs_.size(); ++i) {
    internal::TextRun* run = runs_[visual_to_logical_[i]];
    ui::Range intersection = run->range.Intersect(range);
    if (intersection.IsValid()) {
      DCHECK(!intersection.is_reversed());
      int start_offset = 0;
      hr = ScriptCPtoX(intersection.start() - run->range.start(),
                       false,
                       run->range.length(),
                       run->glyph_count,
                       run->logical_clusters.get(),
                       run->visible_attributes.get(),
                       run->advance_widths.get(),
                       &(run->script_analysis),
                       &start_offset);
      DCHECK(SUCCEEDED(hr));
      int end_offset = 0;
      hr = ScriptCPtoX(intersection.end() - run->range.start(),
                       false,
                       run->range.length(),
                       run->glyph_count,
                       run->logical_clusters.get(),
                       run->visible_attributes.get(),
                       run->advance_widths.get(),
                       &(run->script_analysis),
                       &end_offset);
      DCHECK(SUCCEEDED(hr));
      if (start_offset > end_offset)
        std::swap(start_offset, end_offset);
      Rect rect(run->preceding_run_widths + start_offset, 0,
                end_offset - start_offset, run->font.GetHeight());
      // Center the rect vertically in the display area.
      rect.Offset(0, (display_rect().height() - rect.height()) / 2);
      rect.set_origin(ToViewPoint(rect.origin()));
      // Union this with the last rect if they're adjacent.
      if (!bounds.empty() && rect.SharesEdgeWith(bounds.back())) {
        rect = rect.Union(bounds.back());
        bounds.pop_back();
      }
      bounds.push_back(rect);
    }
  }
  return bounds;
}

void RenderTextWin::ItemizeLogicalText() {
  text_is_dirty_ = false;
  STLDeleteContainerPointers(runs_.begin(), runs_.end());
  runs_.clear();
  if (text().empty())
    return;

  const wchar_t* raw_text = text().c_str();
  const int text_length = text().length();

  HRESULT hr = E_OUTOFMEMORY;
  int script_items_count = 0;
  scoped_array<SCRIPT_ITEM> script_items;
  for (size_t n = kGuessItems; hr == E_OUTOFMEMORY && n < kMaxItems; n *= 2) {
    // Derive the array of Uniscribe script items from the logical text.
    // ScriptItemize always adds a terminal array item so that the length of the
    // last item can be derived from the terminal SCRIPT_ITEM::iCharPos.
    script_items.reset(new SCRIPT_ITEM[n]);
    hr = ScriptItemize(raw_text,
                       text_length,
                       n - 1,
                       &script_control_,
                       &script_state_,
                       script_items.get(),
                       &script_items_count);
  }
  DCHECK(SUCCEEDED(hr));

  if (script_items_count <= 0)
    return;

  // Build the list of runs, merge font/underline styles.
  // TODO(msw): Only break for font changes, not color etc. See TextRun comment.
  // TODO(msw): Apply the overriding selection and composition styles.
  StyleRanges::const_iterator style = style_ranges().begin();
  SCRIPT_ITEM* script_item = script_items.get();
  for (int run_break = 0; run_break < text_length;) {
    internal::TextRun* run = new internal::TextRun();
    run->range.set_start(run_break);
    run->font = !style->underline ? style->font :
        style->font.DeriveFont(0, style->font.GetStyle() | Font::UNDERLINED);
    run->foreground = style->foreground;
    run->strike = style->strike;
    run->script_analysis = script_item->a;

    // Find the range end and advance the structures as needed.
    int script_item_end = (script_item + 1)->iCharPos;
    int style_range_end = style->range.end();
    run_break = std::min(script_item_end, style_range_end);
    if (script_item_end <= style_range_end)
      script_item++;
    if (script_item_end >= style_range_end)
      style++;
    run->range.set_end(run_break);
    runs_.push_back(run);
  }
}

void RenderTextWin::LayoutVisualText(HDC hdc) {
  HRESULT hr = 0;
  std::vector<internal::TextRun*>::const_iterator run_iter;
  for (run_iter = runs_.begin(); run_iter < runs_.end(); ++run_iter) {
    internal::TextRun* run = *run_iter;
    int run_length = run->range.length();
    string16 run_string(text().substr(run->range.start(), run_length));
    const wchar_t* run_text = run_string.c_str();
    // Select the font desired for glyph generation
    SelectObject(hdc, run->font.GetNativeFont());

    run->logical_clusters.reset(new WORD[run_length]);
    run->glyph_count = 0;
    hr = E_OUTOFMEMORY;
    // kGuess comes from: http://msdn.microsoft.com/en-us/library/dd368564.aspx
    const int kGuess = static_cast<int>(1.5 * run_length + 16);
    for (size_t n = kGuess; hr == E_OUTOFMEMORY && n < kMaxGlyphs; n *= 2) {
      run->glyphs.reset(new WORD[n]);
      run->visible_attributes.reset(new SCRIPT_VISATTR[n]);
      hr = ScriptShape(hdc,
                       &script_cache_,
                       run_text,
                       run_length,
                       n,
                       &(run->script_analysis),
                       run->glyphs.get(),
                       run->logical_clusters.get(),
                       run->visible_attributes.get(),
                       &(run->glyph_count));
    }
    DCHECK(SUCCEEDED(hr));

    if (run->glyph_count > 0) {
      run->advance_widths.reset(new int[run->glyph_count]);
      run->offsets.reset(new GOFFSET[run->glyph_count]);
      hr = ScriptPlace(hdc,
                       &script_cache_,
                       run->glyphs.get(),
                       run->glyph_count,
                       run->visible_attributes.get(),
                       &(run->script_analysis),
                       run->advance_widths.get(),
                       run->offsets.get(),
                       &(run->abc_widths));
      DCHECK(SUCCEEDED(hr));
    }
  }

  if (runs_.size() > 0) {
    // Build the array of bidirectional embedding levels.
    scoped_array<BYTE> levels(new BYTE[runs_.size()]);
    for (size_t i = 0; i < runs_.size(); ++i)
      levels[i] = runs_[i]->script_analysis.s.uBidiLevel;

    // Get the maps between visual and logical run indices.
    visual_to_logical_.reset(new int[runs_.size()]);
    logical_to_visual_.reset(new int[runs_.size()]);
    hr = ScriptLayout(runs_.size(),
                      levels.get(),
                      visual_to_logical_.get(),
                      logical_to_visual_.get());
    DCHECK(SUCCEEDED(hr));
  }

  // Precalculate run width information.
  size_t preceding_run_widths = 0;
  for (size_t i = 0; i < runs_.size(); ++i) {
    internal::TextRun* run = runs_[visual_to_logical_[i]];
    run->preceding_run_widths = preceding_run_widths;
    const ABC& abc = run->abc_widths;
    run->width = abc.abcA + abc.abcB + abc.abcC;
    preceding_run_widths += run->width;
  }
  string_width_ = preceding_run_widths;
}

size_t RenderTextWin::GetRunContainingPosition(size_t position) const {
  // Find the text run containing the argument position.
  size_t run = 0;
  for (; run < runs_.size(); ++run)
    if (runs_[run]->range.start() <= position &&
        runs_[run]->range.end() > position)
      break;
  return run;
}

size_t RenderTextWin::GetRunContainingPoint(const Point& point) const {
  // Find the text run containing the argument point (assumed already offset).
  size_t run = 0;
  for (; run < runs_.size(); ++run)
    if (runs_[run]->preceding_run_widths <= point.x() &&
        runs_[run]->preceding_run_widths + runs_[run]->width > point.x())
      break;
  return run;
}

size_t RenderTextWin::IndexOfAdjacentGrapheme(size_t index, bool next) const {
  size_t run_index = GetRunContainingPosition(index);
  internal::TextRun* run = run_index < runs_.size() ? runs_[run_index] : NULL;
  long start = run ? run->range.start() : 0;
  long length = run ? run->range.length() : text().length();
  long ch = index - start;
  WORD cluster = run ? run->logical_clusters[ch] : 0;

  if (!next) {
    do {
      ch--;
    } while (ch >= 0 && run && run->logical_clusters[ch] == cluster);
  } else {
    while (ch < length && run && run->logical_clusters[ch] == cluster)
      ch++;
  }
  return std::max(static_cast<long>(std::min(ch, length) + start), 0L);
}

SelectionModel RenderTextWin::FirstSelectionModelInsideRun(
    internal::TextRun* run) const {
  size_t caret = run->range.start();
  size_t cursor = IndexOfAdjacentGrapheme(caret, true);
  return SelectionModel(cursor, caret, SelectionModel::TRAILING);
}

SelectionModel RenderTextWin::LastSelectionModelInsideRun(
    internal::TextRun* run) const {
  size_t caret = IndexOfAdjacentGrapheme(run->range.end(), false);
  return SelectionModel(caret, caret, SelectionModel::LEADING);
}

SelectionModel RenderTextWin::LeftSelectionModel(
    const SelectionModel& selection) {
  size_t caret = selection.caret_pos();
  SelectionModel::CaretPlacement caret_placement = selection.caret_placement();
  size_t run_index = GetRunContainingPosition(caret);
  DCHECK(run_index < runs_.size());
  internal::TextRun* run = runs_[run_index];

  // If the caret's associated character is in a LTR run.
  if (!run->script_analysis.fRTL) {
    if (caret_placement == SelectionModel::TRAILING)
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    else if (caret > run->range.start()) {
      caret = IndexOfAdjacentGrapheme(caret, false);
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    }
  } else { // The caret's associated character is in a RTL run.
    if (caret_placement == SelectionModel::LEADING) {
      size_t cursor = IndexOfAdjacentGrapheme(caret, true);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    } else if (selection.selection_end() < run->range.end()) {
      caret = IndexOfAdjacentGrapheme(caret, true);
      size_t cursor = IndexOfAdjacentGrapheme(caret, true);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    }
  }

  // The character is at the begin of its run; go to the previous visual run.
  size_t visual_index = logical_to_visual_[run_index];
  if (visual_index == 0)
    return LeftEndSelectionModel();
  internal::TextRun* prev = runs_[visual_to_logical_[visual_index - 1]];
  return prev->script_analysis.fRTL ? FirstSelectionModelInsideRun(prev) :
                                      LastSelectionModelInsideRun(prev);
}

SelectionModel RenderTextWin::RightSelectionModel(
    const SelectionModel& selection) {
  size_t caret = selection.caret_pos();
  SelectionModel::CaretPlacement caret_placement = selection.caret_placement();
  size_t run_index = GetRunContainingPosition(caret);
  DCHECK(run_index < runs_.size());
  internal::TextRun* run = runs_[run_index];

  // If the caret's associated character is in a LTR run.
  if (!run->script_analysis.fRTL) {
    if (caret_placement == SelectionModel::LEADING) {
      size_t cursor = IndexOfAdjacentGrapheme(caret, true);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    } else if (selection.selection_end() < run->range.end()) {
      caret = IndexOfAdjacentGrapheme(caret, true);
      size_t cursor = IndexOfAdjacentGrapheme(caret, true);
      return SelectionModel(cursor, caret, SelectionModel::TRAILING);
    }
  } else { // The caret's associated character is in a RTL run.
    if (caret_placement == SelectionModel::TRAILING)
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    else if (caret > run->range.start()) {
      caret = IndexOfAdjacentGrapheme(caret, false);
      return SelectionModel(caret, caret, SelectionModel::LEADING);
    }
  }

  // The character is at the end of its run; go to the next visual run.
  size_t visual_index = logical_to_visual_[run_index];
  if (visual_index == runs_.size() - 1)
    return RightEndSelectionModel();
  internal::TextRun* next = runs_[visual_to_logical_[visual_index + 1]];
  return next->script_analysis.fRTL ? LastSelectionModelInsideRun(next) :
                                      FirstSelectionModelInsideRun(next);
}

void RenderTextWin::DrawSelection(Canvas* canvas) {
  std::vector<Rect> sel(
      GetSubstringBounds(GetSelectionStart(), GetCursorPosition()));
  SkColor color = focused() ? kFocusedSelectionColor : kUnfocusedSelectionColor;
  for (std::vector<Rect>::const_iterator i = sel.begin(); i < sel.end(); ++i)
    canvas->FillRectInt(color, i->x(), i->y(), i->width(), i->height());
}

void RenderTextWin::DrawVisualText(Canvas* canvas) {
  if (text().empty())
    return;

  CanvasSkia* canvas_skia = canvas->AsCanvasSkia();
  skia::ScopedPlatformPaint scoped_platform_paint(canvas_skia);

  Point offset(ToViewPoint(Point()));
  // TODO(msw): Establish a vertical baseline for strings of mixed font heights.
  size_t height = default_style().font.GetHeight();
  // Center the text vertically in the display area.
  offset.Offset(0, (display_rect().height() - height) / 2);

  SkPaint paint;
  paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setSubpixelText(true);
  paint.setLCDRenderText(true);
  SkPoint point(SkPoint::Make(SkIntToScalar(offset.x()),
      SkIntToScalar(display_rect().height() - offset.y())));
  RECT rect = display_rect().ToRECT();
  scoped_array<SkPoint> pos;
  for (size_t i = 0; i < runs_.size(); ++i) {
    // Get the run specified by the visual-to-logical map.
    internal::TextRun* run = runs_[visual_to_logical_[i]];

    // TODO(msw): Font default/fallback and style integration.
    std::string font(UTF16ToASCII(run->font.GetFontName()));
    SkTypeface::Style style = SkTypeface::kNormal;
    SkTypeface* typeface = SkTypeface::CreateFromName(font.c_str(), style);
    if (typeface) {
      paint.setTypeface(typeface);
      // |paint| adds its own ref. Release the ref from CreateFromName.
      typeface->unref();
    }
    // TODO(msw): Skia font size units? Set OmniboxViewViews gfx::Font size?
    int font_size = run->font.GetFontSize();
    paint.setTextSize(SkFloatToScalar(font_size * kSkiaFontScale));
    paint.setColor(run->foreground);

    // Based on WebCore::skiaDrawText.
    pos.reset(new SkPoint[run->glyph_count]);
    for (int glyph = 0; glyph < run->glyph_count; glyph++) {
        pos[glyph].set(point.x() + run->offsets[glyph].du,
                       point.y() + run->offsets[glyph].dv);
        point.offset(SkIntToScalar(run->advance_widths[glyph]), 0);
    }
    size_t byte_length = run->glyph_count * sizeof(WORD);
    canvas_skia->drawPosText(run->glyphs.get(), byte_length, pos.get(), paint);

    // Draw the strikethrough.
    if (run->strike) {
      Rect bounds(offset, Size(run->width, run->font.GetHeight()));
      SkPaint strike;
      strike.setAntiAlias(true);
      strike.setStyle(SkPaint::kFill_Style);
      strike.setColor(run->foreground);
      strike.setStrokeWidth(kStrikeWidth);
      canvas->AsCanvasSkia()->drawLine(SkIntToScalar(bounds.x()),
                                       SkIntToScalar(bounds.bottom()),
                                       SkIntToScalar(bounds.right()),
                                       SkIntToScalar(bounds.y()),
                                       strike);
    }
    offset.Offset(run->width, 0);
  }
}

void RenderTextWin::DrawCursor(Canvas* canvas) {
  // Paint cursor. Replace cursor is drawn as rectangle for now.
  // TODO(msw): Draw a better cursor with a better indication of association.
  if (cursor_visible() && focused()) {
    Rect r(GetUpdatedCursorBounds());
    canvas->DrawRectInt(kCursorColor, r.x(), r.y(), r.width(), r.height());
  }
}

RenderText* RenderText::CreateRenderText() {
  return new RenderTextWin;
}

}  // namespace gfx
