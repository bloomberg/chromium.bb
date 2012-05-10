// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "ui/base/text/utf16_indexing.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/native_theme.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/shadow_value.h"

namespace {

// All chars are replaced by this char when the password style is set.
// TODO(benrg): GTK uses the first of U+25CF, U+2022, U+2731, U+273A, '*'
// that's available in the font (find_invisible_char() in gtkentry.c).
const char16 kPasswordReplacementChar = '*';

// Default color used for the cursor.
const SkColor kDefaultCursorColor = SK_ColorBLACK;

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
                         const gfx::StyleRange& style_range) {
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

// Converts |gfx::Font::FontStyle| flags to |SkTypeface::Style| flags.
SkTypeface::Style ConvertFontStyleToSkiaTypefaceStyle(int font_style) {
  int skia_style = SkTypeface::kNormal;
  if (font_style & gfx::Font::BOLD)
    skia_style |= SkTypeface::kBold;
  if (font_style & gfx::Font::ITALIC)
    skia_style |= SkTypeface::kItalic;
  return static_cast<SkTypeface::Style>(skia_style);
}

// Given |font| and |display_width|, returns the width of the fade gradient.
int CalculateFadeGradientWidth(const gfx::Font& font, int display_width) {
  // Fade in/out about 2.5 characters of the beginning/end of the string.
  // The .5 here is helpful if one of the characters is a space.
  // Use a quarter of the display width if the display width is very short.
  const int average_character_width = font.GetAverageCharacterWidth();
  const double gradient_width = std::min(average_character_width * 2.5,
                                         display_width / 4.0);
  DCHECK_GE(gradient_width, 0.0);
  return static_cast<int>(floor(gradient_width + 0.5));
}

// Appends to |positions| and |colors| values corresponding to the fade over
// |fade_rect| from color |c0| to color |c1|.
void AddFadeEffect(const gfx::Rect& text_rect,
                   const gfx::Rect& fade_rect,
                   SkColor c0,
                   SkColor c1,
                   std::vector<SkScalar>* positions,
                   std::vector<SkColor>* colors) {
  const SkScalar left = static_cast<SkScalar>(fade_rect.x() - text_rect.x());
  const SkScalar width = static_cast<SkScalar>(fade_rect.width());
  const SkScalar p0 = left / text_rect.width();
  const SkScalar p1 = (left + width) / text_rect.width();
  // Prepend 0.0 to |positions|, as required by Skia.
  if (positions->empty() && p0 != 0.0) {
    positions->push_back(0.0);
    colors->push_back(c0);
  }
  positions->push_back(p0);
  colors->push_back(c0);
  positions->push_back(p1);
  colors->push_back(c1);
}

// Creates a SkShader to fade the text, with |left_part| specifying the left
// fade effect, if any, and |right_part| specifying the right fade effect.
SkShader* CreateFadeShader(const gfx::Rect& text_rect,
                           const gfx::Rect& left_part,
                           const gfx::Rect& right_part,
                           SkColor color) {
  // Fade alpha of 51/255 corresponds to a fade of 0.2 of the original color.
  const SkColor fade_color = SkColorSetA(color, 51);
  std::vector<SkScalar> positions;
  std::vector<SkColor> colors;

  if (!left_part.IsEmpty())
    AddFadeEffect(text_rect, left_part, fade_color, color,
                  &positions, &colors);
  if (!right_part.IsEmpty())
    AddFadeEffect(text_rect, right_part, color, fade_color,
                  &positions, &colors);
  DCHECK(!positions.empty());

  // Terminate |positions| with 1.0, as required by Skia.
  if (positions.back() != 1.0) {
    positions.push_back(1.0);
    colors.push_back(colors.back());
  }

  SkPoint points[2];
  points[0].iset(text_rect.x(), text_rect.y());
  points[1].iset(text_rect.right(), text_rect.y());

  return SkGradientShader::CreateLinear(&points[0], &colors[0], &positions[0],
      colors.size(), SkShader::kClamp_TileMode);
}

}  // namespace

namespace gfx {

namespace internal {

SkiaTextRenderer::SkiaTextRenderer(Canvas* canvas)
    : canvas_skia_(canvas->sk_canvas()),
      started_drawing_(false) {
  DCHECK(canvas_skia_);
  paint_.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint_.setStyle(SkPaint::kFill_Style);
  paint_.setAntiAlias(true);
  paint_.setSubpixelText(true);
  paint_.setLCDRenderText(true);
  bounds_.setEmpty();
}

SkiaTextRenderer::~SkiaTextRenderer() {
  // Work-around for http://crbug.com/122743, where non-ClearType text is
  // rendered with incorrect gamma when using the fade shader. Draw the text
  // to a layer and restore it faded by drawing a rect in kDstIn_Mode mode.
  //
  // TODO(asvitkine): Remove this work-around once the Skia bug is fixed.
  //                  http://code.google.com/p/skia/issues/detail?id=590
  if (deferred_fade_shader_.get()) {
    paint_.setShader(deferred_fade_shader_.get());
    paint_.setXfermodeMode(SkXfermode::kDstIn_Mode);
    canvas_skia_->drawRect(bounds_, paint_);
    canvas_skia_->restore();
  }
}

void SkiaTextRenderer::SetDrawLooper(SkDrawLooper* draw_looper) {
  paint_.setLooper(draw_looper);
}

void SkiaTextRenderer::SetFontSmoothingSettings(bool enable_smoothing,
                                                bool enable_lcd_text) {
  paint_.setAntiAlias(enable_smoothing);
  paint_.setSubpixelText(enable_smoothing);
  paint_.setLCDRenderText(enable_lcd_text);
}

void SkiaTextRenderer::SetTypeface(SkTypeface* typeface) {
  paint_.setTypeface(typeface);
}

void SkiaTextRenderer::SetTextSize(int size) {
  paint_.setTextSize(size);
}

void SkiaTextRenderer::SetFontFamilyWithStyle(const std::string& family,
                                              int style) {
  DCHECK(!family.empty());

  SkTypeface::Style skia_style = ConvertFontStyleToSkiaTypefaceStyle(style);
  SkTypeface* typeface = SkTypeface::CreateFromName(family.c_str(), skia_style);
  SkAutoUnref auto_unref(typeface);
  if (typeface) {
    // |paint_| adds its own ref. So don't |release()| it from the ref ptr here.
    SetTypeface(typeface);

    // Enable fake bold text if bold style is needed but new typeface does not
    // have it.
    paint_.setFakeBoldText((skia_style & SkTypeface::kBold) &&
                           !typeface->isBold());
  }
}

void SkiaTextRenderer::SetForegroundColor(SkColor foreground) {
  paint_.setColor(foreground);
}

void SkiaTextRenderer::SetShader(SkShader* shader, const Rect& bounds) {
  bounds_ = RectToSkRect(bounds);
  paint_.setShader(shader);
}

void SkiaTextRenderer::DrawPosText(const SkPoint* pos,
                                   const uint16* glyphs,
                                   size_t glyph_count) {
  if (!started_drawing_) {
    started_drawing_ = true;
    // Work-around for http://crbug.com/122743, where non-ClearType text is
    // rendered with incorrect gamma when using the fade shader. Draw the text
    // to a layer and restore it faded by drawing a rect in kDstIn_Mode mode.
    //
    // Skip this when there is a looper which seems not working well with
    // deferred paint. Currently a looper is only used for text shadows.
    //
    // TODO(asvitkine): Remove this work-around once the Skia bug is fixed.
    //                  http://code.google.com/p/skia/issues/detail?id=590
    if (!paint_.isLCDRenderText() &&
        paint_.getShader() &&
        !paint_.getLooper()) {
      deferred_fade_shader_ = paint_.getShader();
      paint_.setShader(NULL);
      canvas_skia_->saveLayer(&bounds_, NULL);
    }
  }

  const size_t byte_length = glyph_count * sizeof(glyphs[0]);
  canvas_skia_->drawPosText(&glyphs[0], byte_length, &pos[0], paint_);
}

// Draw underline and strike through text decorations.
// Based on |SkCanvas::DrawTextDecorations()| and constants from:
//   third_party/skia/src/core/SkTextFormatParams.h
void SkiaTextRenderer::DrawDecorations(int x, int y, int width,
                                       const StyleRange& style) {
  if (!style.underline && !style.strike && !style.diagonal_strike)
    return;

  // Fraction of the text size to lower a strike through below the baseline.
  const SkScalar kStrikeThroughOffset = (-SK_Scalar1 * 6 / 21);
  // Fraction of the text size to lower an underline below the baseline.
  const SkScalar kUnderlineOffset = (SK_Scalar1 / 9);
  // Fraction of the text size to use for a strike through or under-line.
  const SkScalar kLineThickness = (SK_Scalar1 / 18);
  // Fraction of the text size to use for a top margin of a diagonal strike.
  const SkScalar kDiagonalStrikeThroughMarginOffset = (SK_Scalar1 / 4);

  SkScalar text_size = paint_.getTextSize();
  SkScalar height = SkScalarMul(text_size, kLineThickness);
  SkRect r;

  r.fLeft = x;
  r.fRight = x + width;

  if (style.underline) {
    SkScalar offset = SkScalarMulAdd(text_size, kUnderlineOffset, y);
    r.fTop = offset;
    r.fBottom = offset + height;
    canvas_skia_->drawRect(r, paint_);
  }
  if (style.strike) {
    SkScalar offset = SkScalarMulAdd(text_size, kStrikeThroughOffset, y);
    r.fTop = offset;
    r.fBottom = offset + height;
    canvas_skia_->drawRect(r, paint_);
  }
  if (style.diagonal_strike) {
    SkScalar offset =
        SkScalarMul(text_size, kDiagonalStrikeThroughMarginOffset);
    SkPaint paint(paint_);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setStrokeWidth(height);
    canvas_skia_->drawLine(
        SkIntToScalar(x), SkIntToScalar(y) - text_size + offset,
        SkIntToScalar(x + width), SkIntToScalar(y),
        paint);
  }
}

}  // namespace internal


StyleRange::StyleRange()
    : foreground(SK_ColorBLACK),
      font_style(gfx::Font::NORMAL),
      strike(false),
      diagonal_strike(false),
      underline(false) {
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
  SetSelectionModel(SelectionModel());

  ResetLayout();
}

void RenderText::SetHorizontalAlignment(HorizontalAlignment alignment) {
  if (horizontal_alignment_ != alignment) {
    horizontal_alignment_ = alignment;
    display_offset_ = Point();
    cached_bounds_and_offset_valid_ = false;
  }
}

void RenderText::SetFontList(const FontList& font_list) {
  font_list_ = font_list;
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
}

void RenderText::SetFontSize(int size) {
  font_list_ = font_list_.DeriveFontListWithSize(size);
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
}

void RenderText::SetCursorEnabled(bool cursor_enabled) {
  cursor_enabled_ = cursor_enabled;
  cached_bounds_and_offset_valid_ = false;
}

const Font& RenderText::GetFont() const {
  return font_list_.GetFonts()[0];
}

void RenderText::ToggleInsertMode() {
  insert_mode_ = !insert_mode_;
  cached_bounds_and_offset_valid_ = false;
}

void RenderText::SetObscured(bool obscured) {
  if (obscured != obscured_) {
    obscured_ = obscured;
    cached_bounds_and_offset_valid_ = false;
    ResetLayout();
  }
}

void RenderText::SetDisplayRect(const Rect& r) {
  if (r.width() != display_rect_.width())
    ResetLayout();
  display_rect_ = r;
  cached_bounds_and_offset_valid_ = false;
}

void RenderText::SetCursorPosition(size_t position) {
  MoveCursorTo(position, false);
}

void RenderText::MoveCursor(BreakType break_type,
                            VisualCursorDirection direction,
                            bool select) {
  SelectionModel position(cursor_position(), selection_model_.caret_affinity());
  // Cancelling a selection moves to the edge of the selection.
  if (break_type != LINE_BREAK && !selection().is_empty() && !select) {
    SelectionModel selection_start = GetSelectionModelForSelectionStart();
    int start_x = GetCursorBounds(selection_start, true).x();
    int cursor_x = GetCursorBounds(position, true).x();
    // Use the selection start if it is left (when |direction| is CURSOR_LEFT)
    // or right (when |direction| is CURSOR_RIGHT) of the selection end.
    if (direction == CURSOR_RIGHT ? start_x > cursor_x : start_x < cursor_x)
      position = selection_start;
    // For word breaks, use the nearest word boundary in the appropriate
    // |direction|.
    if (break_type == WORD_BREAK)
      position = GetAdjacentSelectionModel(position, break_type, direction);
  } else {
    position = GetAdjacentSelectionModel(position, break_type, direction);
  }
  if (select)
    position.set_selection_start(selection().start());
  MoveCursorTo(position);
}

bool RenderText::MoveCursorTo(const SelectionModel& model) {
  // Enforce valid selection model components.
  size_t text_length = text().length();
  ui::Range range(std::min(model.selection().start(), text_length),
                  std::min(model.caret_pos(), text_length));
  // The current model only supports caret positions at valid character indices.
  if (!IsCursorablePosition(range.start()) ||
      !IsCursorablePosition(range.end()))
    return false;
  SelectionModel sel(range, model.caret_affinity());
  bool changed = sel != selection_model_;
  SetSelectionModel(sel);
  return changed;
}

bool RenderText::MoveCursorTo(const Point& point, bool select) {
  SelectionModel position = FindCursorPosition(point);
  if (select)
    position.set_selection_start(selection().start());
  return MoveCursorTo(position);
}

bool RenderText::SelectRange(const ui::Range& range) {
  ui::Range sel(std::min(range.start(), text().length()),
                std::min(range.end(), text().length()));
  if (!IsCursorablePosition(sel.start()) || !IsCursorablePosition(sel.end()))
    return false;
  LogicalCursorDirection affinity =
      (sel.is_reversed() || sel.is_empty()) ? CURSOR_FORWARD : CURSOR_BACKWARD;
  SetSelectionModel(SelectionModel(sel, affinity));
  return true;
}

bool RenderText::IsPointInSelection(const Point& point) {
  if (selection().is_empty())
    return false;
  SelectionModel cursor = FindCursorPosition(point);
  return RangeContainsCaret(
      selection(), cursor.caret_pos(), cursor.caret_affinity());
}

void RenderText::ClearSelection() {
  SetSelectionModel(SelectionModel(cursor_position(),
                                   selection_model_.caret_affinity()));
}

void RenderText::SelectAll() {
  SelectionModel all;
  if (GetTextDirection() == base::i18n::LEFT_TO_RIGHT)
    all = SelectionModel(ui::Range(0, text().length()), CURSOR_FORWARD);
  else
    all = SelectionModel(ui::Range(text().length(), 0), CURSOR_BACKWARD);
  SetSelectionModel(all);
}

void RenderText::SelectWord() {
  if (obscured_) {
    SelectAll();
    return;
  }

  size_t cursor_pos = cursor_position();

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return;

  size_t selection_start = cursor_pos;
  for (; selection_start != 0; --selection_start) {
    if (iter.IsStartOfWord(selection_start) ||
        iter.IsEndOfWord(selection_start))
      break;
  }

  if (selection_start == cursor_pos)
    ++cursor_pos;

  for (; cursor_pos < text().length(); ++cursor_pos)
    if (iter.IsEndOfWord(cursor_pos) || iter.IsStartOfWord(cursor_pos))
      break;

  MoveCursorTo(selection_start, false);
  MoveCursorTo(cursor_pos, true);
}

const ui::Range& RenderText::GetCompositionRange() const {
  return composition_range_;
}

void RenderText::SetCompositionRange(const ui::Range& composition_range) {
  CHECK(!composition_range.IsValid() ||
        ui::Range(0, text_.length()).Contains(composition_range));
  composition_range_.set_end(composition_range.end());
  composition_range_.set_start(composition_range.start());
  ResetLayout();
}

void RenderText::ApplyStyleRange(const StyleRange& style_range) {
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
  ResetLayout();
}

void RenderText::ApplyDefaultStyle() {
  style_ranges_.clear();
  StyleRange style = StyleRange(default_style_);
  style.range.set_end(text_.length());
  style_ranges_.push_back(style);
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
}

VisualCursorDirection RenderText::GetVisualDirectionOfLogicalEnd() {
  return GetTextDirection() == base::i18n::LEFT_TO_RIGHT ?
      CURSOR_RIGHT : CURSOR_LEFT;
}

void RenderText::Draw(Canvas* canvas) {
  TRACE_EVENT0("gfx", "RenderText::Draw");
  {
    TRACE_EVENT0("gfx", "RenderText::EnsureLayout");
    EnsureLayout();
  }

  gfx::Rect clip_rect(display_rect());
  clip_rect.Inset(ShadowValue::GetMargin(text_shadows_));

  canvas->Save();
  canvas->ClipRect(clip_rect);

  if (!text().empty())
    DrawSelection(canvas);

  DrawCursor(canvas);

  if (!text().empty()) {
    TRACE_EVENT0("gfx", "RenderText::Draw draw text");
    DrawVisualText(canvas);
  }
  canvas->Restore();
}

Rect RenderText::GetCursorBounds(const SelectionModel& caret,
                                 bool insert_mode) {
  EnsureLayout();

  size_t caret_pos = caret.caret_pos();
  // In overtype mode, ignore the affinity and always indicate that we will
  // overtype the next character.
  LogicalCursorDirection caret_affinity =
      insert_mode ? caret.caret_affinity() : CURSOR_FORWARD;
  int x = 0, width = 0, height = 0;
  if (caret_pos == (caret_affinity == CURSOR_BACKWARD ? 0 : text().length())) {
    // The caret is attached to the boundary. Always return a zero-width caret,
    // since there is nothing to overtype.
    Size size = GetStringSize();
    if ((GetTextDirection() == base::i18n::RIGHT_TO_LEFT) == (caret_pos == 0))
      x = size.width();
    height = size.height();
  } else {
    size_t grapheme_start = (caret_affinity == CURSOR_FORWARD) ?
        caret_pos : IndexOfAdjacentGrapheme(caret_pos, CURSOR_BACKWARD);
    ui::Range xspan;
    GetGlyphBounds(grapheme_start, &xspan, &height);
    if (insert_mode) {
      x = (caret_affinity == CURSOR_BACKWARD) ? xspan.end() : xspan.start();
    } else {  // overtype mode
      x = xspan.GetMin();
      width = xspan.length();
    }
  }
  height = std::min(height, display_rect().height());
  int y = (display_rect().height() - height) / 2;
  return Rect(ToViewPoint(Point(x, y)), Size(width, height));
}

const Rect& RenderText::GetUpdatedCursorBounds() {
  UpdateCachedBoundsAndOffset();
  return cursor_bounds_;
}

SelectionModel RenderText::GetSelectionModelForSelectionStart() {
  const ui::Range& sel = selection();
  if (sel.is_empty())
    return selection_model_;
  return SelectionModel(sel.start(),
                        sel.is_reversed() ? CURSOR_BACKWARD : CURSOR_FORWARD);
}

void RenderText::SetTextShadows(const std::vector<ShadowValue>& shadows) {
  text_shadows_ = shadows;
}

RenderText::RenderText()
    : horizontal_alignment_(base::i18n::IsRTL() ? ALIGN_RIGHT : ALIGN_LEFT),
      cursor_enabled_(true),
      cursor_visible_(false),
      insert_mode_(true),
      cursor_color_(kDefaultCursorColor),
      focused_(false),
      composition_range_(ui::Range::InvalidRange()),
      obscured_(false),
      fade_head_(false),
      fade_tail_(false),
      background_is_transparent_(false),
      cached_bounds_and_offset_valid_(false) {
}

const Point& RenderText::GetUpdatedDisplayOffset() {
  UpdateCachedBoundsAndOffset();
  return display_offset_;
}

SelectionModel RenderText::GetAdjacentSelectionModel(
    const SelectionModel& current,
    BreakType break_type,
    VisualCursorDirection direction) {
  EnsureLayout();

  if (break_type == LINE_BREAK || text().empty())
    return EdgeSelectionModel(direction);
  if (break_type == CHARACTER_BREAK)
    return AdjacentCharSelectionModel(current, direction);
  DCHECK(break_type == WORD_BREAK);
  return AdjacentWordSelectionModel(current, direction);
}

SelectionModel RenderText::EdgeSelectionModel(
    VisualCursorDirection direction) {
  if (direction == GetVisualDirectionOfLogicalEnd())
    return SelectionModel(text().length(), CURSOR_FORWARD);
  return SelectionModel(0, CURSOR_BACKWARD);
}

void RenderText::SetSelectionModel(const SelectionModel& model) {
  DCHECK_LE(model.selection().GetMax(), text().length());
  selection_model_ = model;
  cached_bounds_and_offset_valid_ = false;
}

string16 RenderText::GetDisplayText() const {
  if (!obscured_)
    return text_;
  size_t obscured_text_length =
      static_cast<size_t>(ui::UTF16IndexToOffset(text_, 0, text_.length()));
  return string16(obscured_text_length, kPasswordReplacementChar);
}

void RenderText::ApplyCompositionAndSelectionStyles(
    StyleRanges* style_ranges) {
  // TODO(msw): This pattern ought to be reconsidered; what about composition
  //            and selection overlaps, retain existing local style features?
  // Apply a composition style override to a copy of the style ranges.
  if (composition_range_.IsValid() && !composition_range_.is_empty()) {
    StyleRange composition_style(default_style_);
    composition_style.underline = true;
    composition_style.range = composition_range_;
    ApplyStyleRangeImpl(style_ranges, composition_style);
  }
  // Apply a selection style override to a copy of the style ranges.
  if (!selection().is_empty()) {
    StyleRange selection_style(default_style_);
    selection_style.foreground = NativeTheme::instance()->GetSystemColor(
        NativeTheme::kColorId_TextfieldSelectionColor);
    selection_style.range = ui::Range(selection().GetMin(),
                                      selection().GetMax());
    ApplyStyleRangeImpl(style_ranges, selection_style);
  }
  // Apply replacement-mode style override to a copy of the style ranges.
  //
  // TODO(xji): NEED TO FIX FOR WINDOWS ASAP. Windows call this function (to
  // apply styles) in ItemizeLogicalText(). In order for the cursor's underline
  // character to be drawn correctly, we will need to re-layout the text. It's
  // not practical to do layout on every cursor blink. We need to fix Windows
  // port to apply styles during drawing phase like Linux port does.
  // http://crbug.com/110109
  if (!insert_mode_ && cursor_visible() && focused()) {
    StyleRange replacement_mode_style(default_style_);
    replacement_mode_style.foreground = NativeTheme::instance()->GetSystemColor(
        NativeTheme::kColorId_TextfieldSelectionColor);
    size_t cursor = cursor_position();
    replacement_mode_style.range.set_start(cursor);
    replacement_mode_style.range.set_end(
        IndexOfAdjacentGrapheme(cursor, CURSOR_FORWARD));
    ApplyStyleRangeImpl(style_ranges, replacement_mode_style);
  }
}

Point RenderText::GetTextOrigin() {
  Point origin = display_rect().origin();
  origin = origin.Add(GetUpdatedDisplayOffset());
  origin = origin.Add(GetAlignmentOffset());
  return origin;
}

Point RenderText::ToTextPoint(const Point& point) {
  return point.Subtract(GetTextOrigin());
}

Point RenderText::ToViewPoint(const Point& point) {
  return point.Add(GetTextOrigin());
}

int RenderText::GetContentWidth() {
  return GetStringSize().width() + (cursor_enabled_ ? 1 : 0);
}

Point RenderText::GetAlignmentOffset() {
  if (horizontal_alignment() != ALIGN_LEFT) {
    int x_offset = display_rect().width() - GetContentWidth();
    if (horizontal_alignment() == ALIGN_CENTER)
      x_offset /= 2;
    return Point(x_offset, 0);
  }
  return Point();
}

Point RenderText::GetOriginForDrawing() {
  Point origin(GetTextOrigin());
  const int height = GetStringSize().height();
  DCHECK_LE(height, display_rect().height());
  // Center the text vertically in the display area.
  origin.Offset(0, (display_rect().height() - height) / 2);
  return origin;
}

void RenderText::ApplyFadeEffects(internal::SkiaTextRenderer* renderer) {
  if (!fade_head() && !fade_tail())
    return;

  const int text_width = GetStringSize().width();
  const int display_width = display_rect().width();

  // If the text fits as-is, no need to fade.
  if (text_width <= display_width)
    return;

  int gradient_width = CalculateFadeGradientWidth(GetFont(), display_width);
  if (gradient_width == 0)
    return;

  bool fade_left = fade_head();
  bool fade_right = fade_tail();
  // Under RTL, |fade_right| == |fade_head|.
  if (GetTextDirection() == base::i18n::RIGHT_TO_LEFT)
    std::swap(fade_left, fade_right);

  gfx::Rect solid_part = display_rect();
  gfx::Rect left_part;
  gfx::Rect right_part;
  if (fade_left) {
    left_part = solid_part;
    left_part.Inset(0, 0, solid_part.width() - gradient_width, 0);
    solid_part.Inset(gradient_width, 0, 0, 0);
  }
  if (fade_right) {
    right_part = solid_part;
    right_part.Inset(solid_part.width() - gradient_width, 0, 0, 0);
    solid_part.Inset(0, 0, gradient_width, 0);
  }

  gfx::Rect text_rect = display_rect();
  text_rect.Inset(GetAlignmentOffset().x(), 0, 0, 0);

  const SkColor color = default_style().foreground;
  SkShader* shader = CreateFadeShader(text_rect, left_part, right_part, color);
  SkAutoUnref auto_unref(shader);
  if (shader) {
    // |renderer| adds its own ref. So don't |release()| it from the ref ptr.
    renderer->SetShader(shader, display_rect());
  }
}

void RenderText::ApplyTextShadows(internal::SkiaTextRenderer* renderer) {
  if (text_shadows_.empty()) {
    renderer->SetDrawLooper(NULL);
    return;
  }

  SkLayerDrawLooper* looper = new SkLayerDrawLooper;
  SkAutoUnref auto_unref(looper);

  looper->addLayer();  // top layer of the original.

  SkLayerDrawLooper::LayerInfo layer_info;
  layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
  layer_info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  layer_info.fColorMode = SkXfermode::kSrc_Mode;

  for (size_t i = 0; i < text_shadows_.size(); ++i) {
    const ShadowValue& shadow = text_shadows_[i];

    layer_info.fOffset.set(SkIntToScalar(shadow.x()),
                           SkIntToScalar(shadow.y()));

    // SkBlurMaskFilter's blur radius defines the range to extend the blur from
    // original mask, which is half of blur amount as defined in ShadowValue.
    SkMaskFilter* blur_mask = SkBlurMaskFilter::Create(
        SkDoubleToScalar(shadow.blur() / 2),
        SkBlurMaskFilter::kNormal_BlurStyle,
        SkBlurMaskFilter::kHighQuality_BlurFlag);
    SkColorFilter* color_filter = SkColorFilter::CreateModeFilter(
        shadow.color(),
        SkXfermode::kSrcIn_Mode);

    SkPaint* paint = looper->addLayer(layer_info);
    SkSafeUnref(paint->setMaskFilter(blur_mask));
    SkSafeUnref(paint->setColorFilter(color_filter));
  }

  renderer->SetDrawLooper(looper);
}

// static
bool RenderText::RangeContainsCaret(const ui::Range& range,
                                    size_t caret_pos,
                                    LogicalCursorDirection caret_affinity) {
  // NB: exploits unsigned wraparound (WG14/N1124 section 6.2.5 paragraph 9).
  size_t adjacent = (caret_affinity == CURSOR_BACKWARD) ?
      caret_pos - 1 : caret_pos + 1;
  return range.Contains(ui::Range(caret_pos, adjacent));
}

void RenderText::MoveCursorTo(size_t position, bool select) {
  size_t cursor = std::min(position, text().length());
  if (IsCursorablePosition(cursor))
    SetSelectionModel(SelectionModel(
        ui::Range(select ? selection().start() : cursor, cursor),
        (cursor == 0) ? CURSOR_FORWARD : CURSOR_BACKWARD));
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
  const int display_width = display_rect_.width();
  const int content_width = GetContentWidth();

  int delta_offset = 0;
  if (content_width <= display_width || !cursor_enabled()) {
    // Don't pan if the text fits in the display width or when the cursor is
    // disabled.
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
  } else if (display_offset_.x() != 0) {
    // Reduce the pan offset to show additional overflow text when the display
    // width increases.
    const int negate_rtl = horizontal_alignment_ == ALIGN_RIGHT ? -1 : 1;
    const int offset = negate_rtl * display_offset_.x();
    if (display_width > (content_width + offset))
      delta_offset = negate_rtl * (display_width - (content_width + offset));
  }

  display_offset_.Offset(delta_offset, 0);
  cursor_bounds_.Offset(delta_offset, 0);
}

void RenderText::DrawSelection(Canvas* canvas) {
  std::vector<Rect> sel = GetSubstringBounds(selection());
  NativeTheme::ColorId color_id = focused() ?
      NativeTheme::kColorId_TextfieldSelectionBackgroundFocused :
      NativeTheme::kColorId_TextfieldSelectionBackgroundUnfocused;
  SkColor color = NativeTheme::instance()->GetSystemColor(color_id);
  for (std::vector<Rect>::const_iterator i = sel.begin(); i < sel.end(); ++i)
    canvas->FillRect(*i, color);
}

void RenderText::DrawCursor(Canvas* canvas) {
  // Paint cursor. Replace cursor is drawn as rectangle for now.
  // TODO(msw): Draw a better cursor with a better indication of association.
  if (cursor_enabled() && cursor_visible() && focused()) {
    const Rect& bounds = GetUpdatedCursorBounds();
    if (bounds.width() != 0)
      canvas->FillRect(bounds, cursor_color_);
    else
      canvas->DrawRect(bounds, cursor_color_);
  }
}

}  // namespace gfx
