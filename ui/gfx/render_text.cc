// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <algorithm>
#include <climits>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/utf16_indexing.h"

namespace gfx {

namespace {

// All chars are replaced by this char when the password style is set.
// TODO(benrg): GTK uses the first of U+25CF, U+2022, U+2731, U+273A, '*'
// that's available in the font (find_invisible_char() in gtkentry.c).
const base::char16 kPasswordReplacementChar = '*';

// Default color used for the text and cursor.
const SkColor kDefaultColor = SK_ColorBLACK;

// Default color used for drawing selection background.
const SkColor kDefaultSelectionBackgroundColor = SK_ColorGRAY;

// Fraction of the text size to lower a strike through below the baseline.
const SkScalar kStrikeThroughOffset = (-SK_Scalar1 * 6 / 21);
// Fraction of the text size to lower an underline below the baseline.
const SkScalar kUnderlineOffset = (SK_Scalar1 / 9);
// Fraction of the text size to use for a strike through or under-line.
const SkScalar kLineThickness = (SK_Scalar1 / 18);
// Fraction of the text size to use for a top margin of a diagonal strike.
const SkScalar kDiagonalStrikeMarginOffset = (SK_Scalar1 / 4);

// Invalid value of baseline.  Assigning this value to |baseline_| causes
// re-calculation of baseline.
const int kInvalidBaseline = INT_MAX;

// Returns the baseline, with which the text best appears vertically centered.
int DetermineBaselineCenteringText(const Rect& display_rect,
                                   const FontList& font_list) {
  const int display_height = display_rect.height();
  const int font_height = font_list.GetHeight();
  // Lower and upper bound of baseline shift as we try to show as much area of
  // text as possible.  In particular case of |display_height| == |font_height|,
  // we do not want to shift the baseline.
  const int min_shift = std::min(0, display_height - font_height);
  const int max_shift = std::abs(display_height - font_height);
  const int baseline = font_list.GetBaseline();
  const int cap_height = font_list.GetCapHeight();
  const int internal_leading = baseline - cap_height;
  // Some platforms don't support getting the cap height, and simply return
  // the entire font ascent from GetCapHeight().  Centering the ascent makes
  // the font look too low, so if GetCapHeight() returns the ascent, center
  // the entire font height instead.
  const int space =
      display_height - ((internal_leading != 0) ? cap_height : font_height);
  const int baseline_shift = space / 2 - internal_leading;
  return baseline + std::max(min_shift, std::min(max_shift, baseline_shift));
}

// Converts |gfx::Font::FontStyle| flags to |SkTypeface::Style| flags.
SkTypeface::Style ConvertFontStyleToSkiaTypefaceStyle(int font_style) {
  int skia_style = SkTypeface::kNormal;
  skia_style |= (font_style & gfx::Font::BOLD) ? SkTypeface::kBold : 0;
  skia_style |= (font_style & gfx::Font::ITALIC) ? SkTypeface::kItalic : 0;
  return static_cast<SkTypeface::Style>(skia_style);
}

// Given |font| and |display_width|, returns the width of the fade gradient.
int CalculateFadeGradientWidth(const Font& font, int display_width) {
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
void AddFadeEffect(const Rect& text_rect,
                   const Rect& fade_rect,
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
skia::RefPtr<SkShader> CreateFadeShader(const Rect& text_rect,
                                        const Rect& left_part,
                                        const Rect& right_part,
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

  return skia::AdoptRef(
      SkGradientShader::CreateLinear(&points[0], &colors[0], &positions[0],
                                     colors.size(), SkShader::kClamp_TileMode));
}

}  // namespace

namespace internal {

// Value of |underline_thickness_| that indicates that underline metrics have
// not been set explicitly.
const SkScalar kUnderlineMetricsNotSet = -1.0f;

SkiaTextRenderer::SkiaTextRenderer(Canvas* canvas)
    : canvas_skia_(canvas->sk_canvas()),
      started_drawing_(false),
      underline_thickness_(kUnderlineMetricsNotSet),
      underline_position_(0.0f) {
  DCHECK(canvas_skia_);
  paint_.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint_.setStyle(SkPaint::kFill_Style);
  paint_.setAntiAlias(true);
  paint_.setSubpixelText(true);
  paint_.setLCDRenderText(true);
  paint_.setHinting(SkPaint::kNormal_Hinting);
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

void SkiaTextRenderer::SetFontHinting(SkPaint::Hinting hinting) {
  paint_.setHinting(hinting);
}

void SkiaTextRenderer::SetTypeface(SkTypeface* typeface) {
  paint_.setTypeface(typeface);
}

void SkiaTextRenderer::SetTextSize(SkScalar size) {
  paint_.setTextSize(size);
}

void SkiaTextRenderer::SetFontFamilyWithStyle(const std::string& family,
                                              int style) {
  DCHECK(!family.empty());

  SkTypeface::Style skia_style = ConvertFontStyleToSkiaTypefaceStyle(style);
  skia::RefPtr<SkTypeface> typeface =
      skia::AdoptRef(SkTypeface::CreateFromName(family.c_str(), skia_style));
  if (typeface) {
    // |paint_| adds its own ref. So don't |release()| it from the ref ptr here.
    SetTypeface(typeface.get());

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

void SkiaTextRenderer::SetUnderlineMetrics(SkScalar thickness,
                                           SkScalar position) {
  underline_thickness_ = thickness;
  underline_position_ = position;
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
      deferred_fade_shader_ = skia::SharePtr(paint_.getShader());
      paint_.setShader(NULL);
      canvas_skia_->saveLayer(&bounds_, NULL);
    }
  }

  const size_t byte_length = glyph_count * sizeof(glyphs[0]);
  canvas_skia_->drawPosText(&glyphs[0], byte_length, &pos[0], paint_);
}

void SkiaTextRenderer::DrawDecorations(int x, int y, int width, bool underline,
                                       bool strike, bool diagonal_strike) {
  if (underline)
    DrawUnderline(x, y, width);
  if (strike)
    DrawStrike(x, y, width);
  if (diagonal_strike)
    DrawDiagonalStrike(x, y, width);
}

void SkiaTextRenderer::DrawUnderline(int x, int y, int width) {
  SkRect r = SkRect::MakeLTRB(x, y + underline_position_, x + width,
                              y + underline_position_ + underline_thickness_);
  if (underline_thickness_ == kUnderlineMetricsNotSet) {
    const SkScalar text_size = paint_.getTextSize();
    r.fTop = SkScalarMulAdd(text_size, kUnderlineOffset, y);
    r.fBottom = r.fTop + SkScalarMul(text_size, kLineThickness);
  }
  canvas_skia_->drawRect(r, paint_);
}

void SkiaTextRenderer::DrawStrike(int x, int y, int width) const {
  const SkScalar text_size = paint_.getTextSize();
  const SkScalar height = SkScalarMul(text_size, kLineThickness);
  const SkScalar offset = SkScalarMulAdd(text_size, kStrikeThroughOffset, y);
  const SkRect r = SkRect::MakeLTRB(x, offset, x + width, offset + height);
  canvas_skia_->drawRect(r, paint_);
}

void SkiaTextRenderer::DrawDiagonalStrike(int x, int y, int width) const {
  const SkScalar text_size = paint_.getTextSize();
  const SkScalar offset = SkScalarMul(text_size, kDiagonalStrikeMarginOffset);

  SkPaint paint(paint_);
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setStrokeWidth(SkScalarMul(text_size, kLineThickness) * 2);
  canvas_skia_->drawLine(x, y, x + width, y - text_size + offset, paint);
}

StyleIterator::StyleIterator(const BreakList<SkColor>& colors,
                             const std::vector<BreakList<bool> >& styles)
    : colors_(colors),
      styles_(styles) {
  color_ = colors_.breaks().begin();
  for (size_t i = 0; i < styles_.size(); ++i)
    style_.push_back(styles_[i].breaks().begin());
}

StyleIterator::~StyleIterator() {}

Range StyleIterator::GetRange() const {
  Range range(colors_.GetRange(color_));
  for (size_t i = 0; i < NUM_TEXT_STYLES; ++i)
    range = range.Intersect(styles_[i].GetRange(style_[i]));
  return range;
}

void StyleIterator::UpdatePosition(size_t position) {
  color_ = colors_.GetBreak(position);
  for (size_t i = 0; i < NUM_TEXT_STYLES; ++i)
    style_[i] = styles_[i].GetBreak(position);
}

LineSegment::LineSegment() : run(0) {}

LineSegment::~LineSegment() {}

Line::Line() : preceding_heights(0), baseline(0) {}

Line::~Line() {}

}  // namespace internal

RenderText::~RenderText() {
}

void RenderText::SetText(const base::string16& text) {
  DCHECK(!composition_range_.IsValid());
  if (text_ == text)
    return;
  text_ = text;

  // Adjust ranged styles and colors to accommodate a new text length.
  const size_t text_length = text_.length();
  colors_.SetMax(text_length);
  for (size_t style = 0; style < NUM_TEXT_STYLES; ++style)
    styles_[style].SetMax(text_length);
  cached_bounds_and_offset_valid_ = false;

  // Reset selection model. SetText should always followed by SetSelectionModel
  // or SetCursorPosition in upper layer.
  SetSelectionModel(SelectionModel());

  // Invalidate the cached text direction if it depends on the text contents.
  if (directionality_mode_ == DIRECTIONALITY_FROM_TEXT)
    text_direction_ = base::i18n::UNKNOWN_DIRECTION;

  obscured_reveal_index_ = -1;
  UpdateLayoutText();
  ResetLayout();
}

void RenderText::SetHorizontalAlignment(HorizontalAlignment alignment) {
  if (horizontal_alignment_ != alignment) {
    horizontal_alignment_ = alignment;
    display_offset_ = Vector2d();
    cached_bounds_and_offset_valid_ = false;
  }
}

void RenderText::SetFontList(const FontList& font_list) {
  font_list_ = font_list;
  baseline_ = kInvalidBaseline;
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
}

void RenderText::SetFont(const Font& font) {
  SetFontList(FontList(font));
}

void RenderText::SetFontSize(int size) {
  SetFontList(font_list_.DeriveFontListWithSize(size));
}

const Font& RenderText::GetPrimaryFont() const {
  return font_list_.GetPrimaryFont();
}

void RenderText::SetCursorEnabled(bool cursor_enabled) {
  cursor_enabled_ = cursor_enabled;
  cached_bounds_and_offset_valid_ = false;
}

void RenderText::ToggleInsertMode() {
  insert_mode_ = !insert_mode_;
  cached_bounds_and_offset_valid_ = false;
}

void RenderText::SetObscured(bool obscured) {
  if (obscured != obscured_) {
    obscured_ = obscured;
    obscured_reveal_index_ = -1;
    cached_bounds_and_offset_valid_ = false;
    UpdateLayoutText();
    ResetLayout();
  }
}

void RenderText::SetObscuredRevealIndex(int index) {
  if (obscured_reveal_index_ == index)
    return;

  obscured_reveal_index_ = index;
  cached_bounds_and_offset_valid_ = false;
  UpdateLayoutText();
  ResetLayout();
}

void RenderText::SetMultiline(bool multiline) {
  if (multiline != multiline_) {
    multiline_ = multiline;
    cached_bounds_and_offset_valid_ = false;
    lines_.clear();
  }
}

void RenderText::SetDisplayRect(const Rect& r) {
  display_rect_ = r;
  baseline_ = kInvalidBaseline;
  cached_bounds_and_offset_valid_ = false;
  lines_.clear();
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
  Range range(std::min(model.selection().start(), text_length),
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

bool RenderText::SelectRange(const Range& range) {
  Range sel(std::min(range.start(), text().length()),
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

void RenderText::SelectAll(bool reversed) {
  const size_t length = text().length();
  const Range all = reversed ? Range(length, 0) : Range(0, length);
  const bool success = SelectRange(all);
  DCHECK(success);
}

void RenderText::SelectWord() {
  if (obscured_) {
    SelectAll(false);
    return;
  }

  size_t selection_max = selection().GetMax();

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return;

  size_t selection_min = selection().GetMin();
  if (selection_min == text().length() && selection_min != 0)
    --selection_min;

  for (; selection_min != 0; --selection_min) {
    if (iter.IsStartOfWord(selection_min) ||
        iter.IsEndOfWord(selection_min))
      break;
  }

  if (selection_min == selection_max && selection_max != text().length())
    ++selection_max;

  for (; selection_max < text().length(); ++selection_max)
    if (iter.IsEndOfWord(selection_max) || iter.IsStartOfWord(selection_max))
      break;

  const bool reversed = selection().is_reversed();
  MoveCursorTo(reversed ? selection_max : selection_min, false);
  MoveCursorTo(reversed ? selection_min : selection_max, true);
}

const Range& RenderText::GetCompositionRange() const {
  return composition_range_;
}

void RenderText::SetCompositionRange(const Range& composition_range) {
  CHECK(!composition_range.IsValid() ||
        Range(0, text_.length()).Contains(composition_range));
  composition_range_.set_end(composition_range.end());
  composition_range_.set_start(composition_range.start());
  ResetLayout();
}

void RenderText::SetColor(SkColor value) {
  colors_.SetValue(value);

#if defined(OS_WIN)
  // TODO(msw): Windows applies colors and decorations in the layout process.
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
#endif
}

void RenderText::ApplyColor(SkColor value, const Range& range) {
  colors_.ApplyValue(value, range);

#if defined(OS_WIN)
  // TODO(msw): Windows applies colors and decorations in the layout process.
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
#endif
}

void RenderText::SetStyle(TextStyle style, bool value) {
  styles_[style].SetValue(value);

  // Only invalidate the layout on font changes; not for colors or decorations.
  bool invalidate = (style == BOLD) || (style == ITALIC);
#if defined(OS_WIN)
  // TODO(msw): Windows applies colors and decorations in the layout process.
  invalidate = true;
#endif
  if (invalidate) {
    cached_bounds_and_offset_valid_ = false;
    ResetLayout();
  }
}

void RenderText::ApplyStyle(TextStyle style, bool value, const Range& range) {
  styles_[style].ApplyValue(value, range);

  // Only invalidate the layout on font changes; not for colors or decorations.
  bool invalidate = (style == BOLD) || (style == ITALIC);
#if defined(OS_WIN)
  // TODO(msw): Windows applies colors and decorations in the layout process.
  invalidate = true;
#endif
  if (invalidate) {
    cached_bounds_and_offset_valid_ = false;
    ResetLayout();
  }
}

bool RenderText::GetStyle(TextStyle style) const {
  return (styles_[style].breaks().size() == 1) &&
      styles_[style].breaks().front().second;
}

void RenderText::SetDirectionalityMode(DirectionalityMode mode) {
  if (mode == directionality_mode_)
    return;

  directionality_mode_ = mode;
  text_direction_ = base::i18n::UNKNOWN_DIRECTION;
  cached_bounds_and_offset_valid_ = false;
  ResetLayout();
}

base::i18n::TextDirection RenderText::GetTextDirection() {
  if (text_direction_ == base::i18n::UNKNOWN_DIRECTION) {
    switch (directionality_mode_) {
      case DIRECTIONALITY_FROM_TEXT:
        // Derive the direction from the display text, which differs from text()
        // in the case of obscured (password) textfields.
        text_direction_ =
            base::i18n::GetFirstStrongCharacterDirection(GetLayoutText());
        break;
      case DIRECTIONALITY_FROM_UI:
        text_direction_ = base::i18n::IsRTL() ? base::i18n::RIGHT_TO_LEFT :
                                                base::i18n::LEFT_TO_RIGHT;
        break;
      case DIRECTIONALITY_FORCE_LTR:
        text_direction_ = base::i18n::LEFT_TO_RIGHT;
        break;
      case DIRECTIONALITY_FORCE_RTL:
        text_direction_ = base::i18n::RIGHT_TO_LEFT;
        break;
      default:
        NOTREACHED();
    }
  }

  return text_direction_;
}

VisualCursorDirection RenderText::GetVisualDirectionOfLogicalEnd() {
  return GetTextDirection() == base::i18n::LEFT_TO_RIGHT ?
      CURSOR_RIGHT : CURSOR_LEFT;
}

SizeF RenderText::GetStringSizeF() {
  const Size size = GetStringSize();
  return SizeF(size.width(), size.height());
}

int RenderText::GetContentWidth() {
  return GetStringSize().width() + (cursor_enabled_ ? 1 : 0);
}

int RenderText::GetBaseline() {
  if (baseline_ == kInvalidBaseline)
    baseline_ = DetermineBaselineCenteringText(display_rect(), font_list());
  DCHECK_NE(kInvalidBaseline, baseline_);
  return baseline_;
}

void RenderText::Draw(Canvas* canvas) {
  EnsureLayout();

  if (clip_to_display_rect()) {
    Rect clip_rect(display_rect());
    clip_rect.Inset(ShadowValue::GetMargin(text_shadows_));

    canvas->Save();
    canvas->ClipRect(clip_rect);
  }

  if (!text().empty() && focused())
    DrawSelection(canvas);

  if (cursor_enabled() && cursor_visible() && focused())
    DrawCursor(canvas, selection_model_);

  if (!text().empty())
    DrawVisualText(canvas);

  if (clip_to_display_rect())
    canvas->Restore();
}

void RenderText::DrawCursor(Canvas* canvas, const SelectionModel& position) {
  // Paint cursor. Replace cursor is drawn as rectangle for now.
  // TODO(msw): Draw a better cursor with a better indication of association.
  canvas->FillRect(GetCursorBounds(position, true), cursor_color_);
}

void RenderText::DrawSelectedTextForDrag(Canvas* canvas) {
  EnsureLayout();
  const std::vector<Rect> sel = GetSubstringBounds(selection());

  // Override the selection color with black, and force the background to be
  // transparent so that it's rendered without subpixel antialiasing.
  const bool saved_background_is_transparent = background_is_transparent();
  const SkColor saved_selection_color = selection_color();
  set_background_is_transparent(true);
  set_selection_color(SK_ColorBLACK);

  for (size_t i = 0; i < sel.size(); ++i) {
    canvas->Save();
    canvas->ClipRect(sel[i]);
    DrawVisualText(canvas);
    canvas->Restore();
  }

  // Restore saved transparency and selection color.
  set_selection_color(saved_selection_color);
  set_background_is_transparent(saved_background_is_transparent);
}

Rect RenderText::GetCursorBounds(const SelectionModel& caret,
                                 bool insert_mode) {
  // TODO(ckocagil): Support multiline. This function should return the height
  //                 of the line the cursor is on. |GetStringSize()| now returns
  //                 the multiline size, eliminate its use here.

  EnsureLayout();

  size_t caret_pos = caret.caret_pos();
  DCHECK(IsCursorablePosition(caret_pos));
  // In overtype mode, ignore the affinity and always indicate that we will
  // overtype the next character.
  LogicalCursorDirection caret_affinity =
      insert_mode ? caret.caret_affinity() : CURSOR_FORWARD;
  int x = 0, width = 1;
  Size size = GetStringSize();
  if (caret_pos == (caret_affinity == CURSOR_BACKWARD ? 0 : text().length())) {
    // The caret is attached to the boundary. Always return a 1-dip width caret,
    // since there is nothing to overtype.
    if ((GetTextDirection() == base::i18n::RIGHT_TO_LEFT) == (caret_pos == 0))
      x = size.width();
  } else {
    size_t grapheme_start = (caret_affinity == CURSOR_FORWARD) ?
        caret_pos : IndexOfAdjacentGrapheme(caret_pos, CURSOR_BACKWARD);
    Range xspan(GetGlyphBounds(grapheme_start));
    if (insert_mode) {
      x = (caret_affinity == CURSOR_BACKWARD) ? xspan.end() : xspan.start();
    } else {  // overtype mode
      x = xspan.GetMin();
      width = xspan.length();
    }
  }
  return Rect(ToViewPoint(Point(x, 0)), Size(width, size.height()));
}

const Rect& RenderText::GetUpdatedCursorBounds() {
  UpdateCachedBoundsAndOffset();
  return cursor_bounds_;
}

size_t RenderText::IndexOfAdjacentGrapheme(size_t index,
                                           LogicalCursorDirection direction) {
  if (index > text().length())
    return text().length();

  EnsureLayout();

  if (direction == CURSOR_FORWARD) {
    while (index < text().length()) {
      index++;
      if (IsCursorablePosition(index))
        return index;
    }
    return text().length();
  }

  while (index > 0) {
    index--;
    if (IsCursorablePosition(index))
      return index;
  }
  return 0;
}

SelectionModel RenderText::GetSelectionModelForSelectionStart() {
  const Range& sel = selection();
  if (sel.is_empty())
    return selection_model_;
  return SelectionModel(sel.start(),
                        sel.is_reversed() ? CURSOR_BACKWARD : CURSOR_FORWARD);
}

void RenderText::SetTextShadows(const ShadowValues& shadows) {
  text_shadows_ = shadows;
}

RenderText::RenderText()
    : horizontal_alignment_(base::i18n::IsRTL() ? ALIGN_RIGHT : ALIGN_LEFT),
      directionality_mode_(DIRECTIONALITY_FROM_TEXT),
      text_direction_(base::i18n::UNKNOWN_DIRECTION),
      cursor_enabled_(true),
      cursor_visible_(false),
      insert_mode_(true),
      cursor_color_(kDefaultColor),
      selection_color_(kDefaultColor),
      selection_background_focused_color_(kDefaultSelectionBackgroundColor),
      focused_(false),
      composition_range_(Range::InvalidRange()),
      colors_(kDefaultColor),
      styles_(NUM_TEXT_STYLES),
      composition_and_selection_styles_applied_(false),
      obscured_(false),
      obscured_reveal_index_(-1),
      truncate_length_(0),
      multiline_(false),
      fade_head_(false),
      fade_tail_(false),
      background_is_transparent_(false),
      clip_to_display_rect_(true),
      baseline_(kInvalidBaseline),
      cached_bounds_and_offset_valid_(false) {
}

const Vector2d& RenderText::GetUpdatedDisplayOffset() {
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

const base::string16& RenderText::GetLayoutText() const {
  return layout_text_.empty() ? text_ : layout_text_;
}

const BreakList<size_t>& RenderText::GetLineBreaks() {
  if (line_breaks_.max() != 0)
    return line_breaks_;

  const base::string16& layout_text = GetLayoutText();
  const size_t text_length = layout_text.length();
  line_breaks_.SetValue(0);
  line_breaks_.SetMax(text_length);
  base::i18n::BreakIterator iter(layout_text,
                                 base::i18n::BreakIterator::BREAK_LINE);
  const bool success = iter.Init();
  DCHECK(success);
  if (success) {
    do {
      line_breaks_.ApplyValue(iter.pos(), Range(iter.pos(), text_length));
    } while (iter.Advance());
  }
  return line_breaks_;
}

void RenderText::ApplyCompositionAndSelectionStyles() {
  // Save the underline and color breaks to undo the temporary styles later.
  DCHECK(!composition_and_selection_styles_applied_);
  saved_colors_ = colors_;
  saved_underlines_ = styles_[UNDERLINE];

  // Apply an underline to the composition range in |underlines|.
  if (composition_range_.IsValid() && !composition_range_.is_empty())
    styles_[UNDERLINE].ApplyValue(true, composition_range_);

  // Apply the selected text color to the [un-reversed] selection range.
  if (!selection().is_empty() && focused()) {
    const Range range(selection().GetMin(), selection().GetMax());
    colors_.ApplyValue(selection_color_, range);
  }
  composition_and_selection_styles_applied_ = true;
}

void RenderText::UndoCompositionAndSelectionStyles() {
  // Restore the underline and color breaks to undo the temporary styles.
  DCHECK(composition_and_selection_styles_applied_);
  colors_ = saved_colors_;
  styles_[UNDERLINE] = saved_underlines_;
  composition_and_selection_styles_applied_ = false;
}

Vector2d RenderText::GetLineOffset(size_t line_number) {
  Vector2d offset = display_rect().OffsetFromOrigin();
  // TODO(ckocagil): Apply the display offset for multiline scrolling.
  if (!multiline())
    offset.Add(GetUpdatedDisplayOffset());
  else
    offset.Add(Vector2d(0, lines_[line_number].preceding_heights));
  offset.Add(GetAlignmentOffset(line_number));
  return offset;
}

Point RenderText::ToTextPoint(const Point& point) {
  return point - GetLineOffset(0);
  // TODO(ckocagil): Convert multiline view space points to text space.
}

Point RenderText::ToViewPoint(const Point& point) {
  if (!multiline())
    return point + GetLineOffset(0);

  // TODO(ckocagil): Traverse individual line segments for RTL support.
  DCHECK(!lines_.empty());
  int x = point.x();
  size_t line = 0;
  for (; line < lines_.size() && x > lines_[line].size.width(); ++line)
    x -= lines_[line].size.width();
  return Point(x, point.y()) + GetLineOffset(line);
}

std::vector<Rect> RenderText::TextBoundsToViewBounds(const Range& x) {
  std::vector<Rect> rects;

  if (!multiline()) {
    rects.push_back(Rect(ToViewPoint(Point(x.GetMin(), 0)),
                         Size(x.length(), GetStringSize().height())));
    return rects;
  }

  EnsureLayout();

  // Each line segment keeps its position in text coordinates. Traverse all line
  // segments and if the segment intersects with the given range, add the view
  // rect corresponding to the intersection to |rects|.
  for (size_t line = 0; line < lines_.size(); ++line) {
    int line_x = 0;
    const Vector2d offset = GetLineOffset(line);
    for (size_t i = 0; i < lines_[line].segments.size(); ++i) {
      const internal::LineSegment* segment = &lines_[line].segments[i];
      const Range intersection = segment->x_range.Intersect(x);
      if (!intersection.is_empty()) {
        Rect rect(line_x + intersection.start() - segment->x_range.start(),
                  0, intersection.length(), lines_[line].size.height());
        rects.push_back(rect + offset);
      }
      line_x += segment->x_range.length();
    }
  }

  return rects;
}

Vector2d RenderText::GetAlignmentOffset(size_t line_number) {
  // TODO(ckocagil): Enable |lines_| usage in other platforms.
#if defined(OS_WIN)
  DCHECK_LT(line_number, lines_.size());
#endif
  Vector2d offset;
  if (horizontal_alignment_ != ALIGN_LEFT) {
#if defined(OS_WIN)
    const int width = lines_[line_number].size.width() +
        (cursor_enabled_ ? 1 : 0);
#else
    const int width = GetContentWidth();
#endif
    offset.set_x(display_rect().width() - width);
    if (horizontal_alignment_ == ALIGN_CENTER)
      offset.set_x(offset.x() / 2);
  }

  // Vertically center the text.
  if (multiline_) {
    const int text_height = lines_.back().preceding_heights +
        lines_.back().size.height();
    offset.set_y((display_rect_.height() - text_height) / 2);
  } else {
    offset.set_y(GetBaseline() - GetLayoutTextBaseline());
  }

  return offset;
}

void RenderText::ApplyFadeEffects(internal::SkiaTextRenderer* renderer) {
  if (multiline() || (!fade_head() && !fade_tail()))
    return;

  const int display_width = display_rect().width();

  // If the text fits as-is, no need to fade.
  if (GetStringSize().width() <= display_width)
    return;

  int gradient_width = CalculateFadeGradientWidth(GetPrimaryFont(),
                                                  display_width);
  if (gradient_width == 0)
    return;

  bool fade_left = fade_head();
  bool fade_right = fade_tail();
  // Under RTL, |fade_right| == |fade_head|.
  // TODO(asvitkine): This is currently not based on GetTextDirection() because
  //                  RenderTextWin does not return a direction that's based on
  //                  the text content.
  if (horizontal_alignment_ == ALIGN_RIGHT)
    std::swap(fade_left, fade_right);

  Rect solid_part = display_rect();
  Rect left_part;
  Rect right_part;
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

  Rect text_rect = display_rect();
  text_rect.Inset(GetAlignmentOffset(0).x(), 0, 0, 0);

  // TODO(msw): Use the actual text colors corresponding to each faded part.
  skia::RefPtr<SkShader> shader = CreateFadeShader(
      text_rect, left_part, right_part, colors_.breaks().front().second);
  if (shader)
    renderer->SetShader(shader.get(), display_rect());
}

void RenderText::ApplyTextShadows(internal::SkiaTextRenderer* renderer) {
  skia::RefPtr<SkDrawLooper> looper = CreateShadowDrawLooper(text_shadows_);
  renderer->SetDrawLooper(looper.get());
}

// static
bool RenderText::RangeContainsCaret(const Range& range,
                                    size_t caret_pos,
                                    LogicalCursorDirection caret_affinity) {
  // NB: exploits unsigned wraparound (WG14/N1124 section 6.2.5 paragraph 9).
  size_t adjacent = (caret_affinity == CURSOR_BACKWARD) ?
      caret_pos - 1 : caret_pos + 1;
  return range.Contains(Range(caret_pos, adjacent));
}

void RenderText::MoveCursorTo(size_t position, bool select) {
  size_t cursor = std::min(position, text().length());
  if (IsCursorablePosition(cursor))
    SetSelectionModel(SelectionModel(
        Range(select ? selection().start() : cursor, cursor),
        (cursor == 0) ? CURSOR_FORWARD : CURSOR_BACKWARD));
}

void RenderText::UpdateLayoutText() {
  layout_text_.clear();
  line_breaks_.SetMax(0);

  if (obscured_) {
    size_t obscured_text_length =
        static_cast<size_t>(gfx::UTF16IndexToOffset(text_, 0, text_.length()));
    layout_text_.assign(obscured_text_length, kPasswordReplacementChar);

    if (obscured_reveal_index_ >= 0 &&
        obscured_reveal_index_ < static_cast<int>(text_.length())) {
      // Gets the index range in |text_| to be revealed.
      size_t start = obscured_reveal_index_;
      U16_SET_CP_START(text_.data(), 0, start);
      size_t end = start;
      UChar32 unused_char;
      U16_NEXT(text_.data(), end, text_.length(), unused_char);

      // Gets the index in |layout_text_| to be replaced.
      const size_t cp_start =
          static_cast<size_t>(gfx::UTF16IndexToOffset(text_, 0, start));
      if (layout_text_.length() > cp_start)
        layout_text_.replace(cp_start, 1, text_.substr(start, end - start));
    }
  }

  const base::string16& text = obscured_ ? layout_text_ : text_;
  if (truncate_length_ > 0 && truncate_length_ < text.length()) {
    // Truncate the text at a valid character break and append an ellipsis.
    icu::StringCharacterIterator iter(text.c_str());
    iter.setIndex32(truncate_length_ - 1);
    layout_text_.assign(text.substr(0, iter.getIndex()) + gfx::kEllipsisUTF16);
  }
}

void RenderText::UpdateCachedBoundsAndOffset() {
  if (cached_bounds_and_offset_valid_)
    return;

  // TODO(ckocagil): Add support for scrolling multiline text.

  // First, set the valid flag true to calculate the current cursor bounds using
  // the stale |display_offset_|. Applying |delta_offset| at the end of this
  // function will set |cursor_bounds_| and |display_offset_| to correct values.
  cached_bounds_and_offset_valid_ = true;
  cursor_bounds_ = GetCursorBounds(selection_model_, insert_mode_);

  // Update |display_offset_| to ensure the current cursor is visible.
  const int display_width = display_rect_.width();
  const int content_width = GetContentWidth();

  int delta_x = 0;
  if (content_width <= display_width || !cursor_enabled()) {
    // Don't pan if the text fits in the display width or when the cursor is
    // disabled.
    delta_x = -display_offset_.x();
  } else if (cursor_bounds_.right() > display_rect_.right()) {
    // TODO(xji): when the character overflow is a RTL character, currently, if
    // we pan cursor at the rightmost position, the entered RTL character is not
    // displayed. Should pan cursor to show the last logical characters.
    //
    // Pan to show the cursor when it overflows to the right.
    delta_x = display_rect_.right() - cursor_bounds_.right();
  } else if (cursor_bounds_.x() < display_rect_.x()) {
    // TODO(xji): have similar problem as above when overflow character is a
    // LTR character.
    //
    // Pan to show the cursor when it overflows to the left.
    delta_x = display_rect_.x() - cursor_bounds_.x();
  } else if (display_offset_.x() != 0) {
    // Reduce the pan offset to show additional overflow text when the display
    // width increases.
    const int negate_rtl = horizontal_alignment_ == ALIGN_RIGHT ? -1 : 1;
    const int offset = negate_rtl * display_offset_.x();
    if (display_width > (content_width + offset)) {
      delta_x = negate_rtl * (display_width - (content_width + offset));
    }
  }

  Vector2d delta_offset(delta_x, 0);
  display_offset_ += delta_offset;
  cursor_bounds_ += delta_offset;
}

void RenderText::DrawSelection(Canvas* canvas) {
  const std::vector<Rect> sel = GetSubstringBounds(selection());
  for (std::vector<Rect>::const_iterator i = sel.begin(); i < sel.end(); ++i)
    canvas->FillRect(*i, selection_background_focused_color_);
}

}  // namespace gfx
