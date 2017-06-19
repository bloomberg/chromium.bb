/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/LayoutSVGInlineText.h"

#include "core/css/CSSFontSelector.h"
#include "core/css/FontSize.h"
#include "core/dom/StyleEngine.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/text/BidiCharacterRun.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"

namespace blink {

// Turn tabs, newlines and carriage returns into spaces. In the future this
// should be removed in favor of letting the generic white-space code handle
// this.
static RefPtr<StringImpl> NormalizeWhitespace(RefPtr<StringImpl> string) {
  RefPtr<StringImpl> new_string = string->Replace('\t', ' ');
  new_string = new_string->Replace('\n', ' ');
  new_string = new_string->Replace('\r', ' ');
  return new_string;
}

LayoutSVGInlineText::LayoutSVGInlineText(Node* n, PassRefPtr<StringImpl> string)
    : LayoutText(n, NormalizeWhitespace(std::move(string))),
      scaling_factor_(1) {}

void LayoutSVGInlineText::SetTextInternal(RefPtr<StringImpl> text) {
  LayoutText::SetTextInternal(std::move(text));
  if (LayoutSVGText* text_layout_object =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this))
    text_layout_object->SubtreeTextDidChange();
}

void LayoutSVGInlineText::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  LayoutText::StyleDidChange(diff, old_style);
  UpdateScaledFont();

  bool new_preserves =
      Style() ? Style()->WhiteSpace() == EWhiteSpace::kPre : false;
  bool old_preserves =
      old_style ? old_style->WhiteSpace() == EWhiteSpace::kPre : false;
  if (old_preserves != new_preserves) {
    SetText(OriginalText(), true);
    return;
  }

  if (!diff.NeedsFullLayout())
    return;

  // The text metrics may be influenced by style changes.
  if (LayoutSVGText* text_layout_object =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this)) {
    text_layout_object->SetNeedsTextMetricsUpdate();
    text_layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kStyleChange);
  }
}

InlineTextBox* LayoutSVGInlineText::CreateTextBox(int start,
                                                  unsigned short length) {
  InlineTextBox* box =
      new SVGInlineTextBox(LineLayoutItem(this), start, length);
  box->SetHasVirtualLogicalHeight();
  return box;
}

LayoutRect LayoutSVGInlineText::LocalCaretRect(InlineBox* box,
                                               int caret_offset,
                                               LayoutUnit*) {
  if (!box || !box->IsInlineTextBox())
    return LayoutRect();

  InlineTextBox* text_box = ToInlineTextBox(box);
  if (static_cast<unsigned>(caret_offset) < text_box->Start() ||
      static_cast<unsigned>(caret_offset) > text_box->Start() + text_box->Len())
    return LayoutRect();

  // Use the edge of the selection rect to determine the caret rect.
  if (static_cast<unsigned>(caret_offset) <
      text_box->Start() + text_box->Len()) {
    LayoutRect rect =
        text_box->LocalSelectionRect(caret_offset, caret_offset + 1);
    LayoutUnit x = box->IsLeftToRightDirection() ? rect.X() : rect.MaxX();
    return LayoutRect(x, rect.Y(), GetFrameView()->CaretWidth(), rect.Height());
  }

  LayoutRect rect =
      text_box->LocalSelectionRect(caret_offset - 1, caret_offset);
  LayoutUnit x = box->IsLeftToRightDirection() ? rect.MaxX() : rect.X();
  return LayoutRect(x, rect.Y(), GetFrameView()->CaretWidth(), rect.Height());
}

FloatRect LayoutSVGInlineText::FloatLinesBoundingBox() const {
  FloatRect bounding_box;
  for (InlineTextBox* box : InlineTextBoxesOf(*this))
    bounding_box.Unite(FloatRect(box->FrameRect()));
  return bounding_box;
}

LayoutRect LayoutSVGInlineText::LinesBoundingBox() const {
  return EnclosingLayoutRect(FloatLinesBoundingBox());
}

bool LayoutSVGInlineText::CharacterStartsNewTextChunk(int position) const {
  DCHECK_GE(position, 0);
  DCHECK_LT(position, static_cast<int>(TextLength()));

  // Each <textPath> element starts a new text chunk, regardless of any x/y
  // values.
  if (!position && Parent()->IsSVGTextPath() && !PreviousSibling())
    return true;

  const SVGCharacterDataMap::const_iterator it =
      character_data_map_.find(static_cast<unsigned>(position + 1));
  if (it == character_data_map_.end())
    return false;

  return it->value.HasX() || it->value.HasY();
}

PositionWithAffinity LayoutSVGInlineText::PositionForPoint(
    const LayoutPoint& point) {
  if (!HasTextBoxes() || !TextLength())
    return CreatePositionWithAffinity(0);

  DCHECK(scaling_factor_);

  const SimpleFontData* font_data = scaled_font_.PrimaryFont();
  DCHECK(font_data);
  float baseline =
      font_data ? font_data->GetFontMetrics().FloatAscent() / scaling_factor_
                : 0;

  LayoutBlock* containing_block = this->ContainingBlock();
  DCHECK(containing_block);

  // Map local point to absolute point, as the character origins stored in the
  // text fragments use absolute coordinates.
  FloatPoint absolute_point(point);
  absolute_point.MoveBy(containing_block->Location());

  float closest_distance = std::numeric_limits<float>::max();
  float closest_distance_position = 0;
  const SVGTextFragment* closest_distance_fragment = nullptr;
  SVGInlineTextBox* closest_distance_box = nullptr;

  for (InlineTextBox* box : InlineTextBoxesOf(*this)) {
    if (!box->IsSVGInlineTextBox())
      continue;

    SVGInlineTextBox* text_box = ToSVGInlineTextBox(box);
    for (const SVGTextFragment& fragment : text_box->TextFragments()) {
      FloatRect fragment_rect = fragment.BoundingBox(baseline);

      float distance = 0;
      if (!fragment_rect.Contains(absolute_point))
        distance = fragment_rect.SquaredDistanceTo(absolute_point);

      if (distance <= closest_distance) {
        closest_distance = distance;
        closest_distance_box = text_box;
        closest_distance_fragment = &fragment;
        closest_distance_position = fragment_rect.X();
      }
    }
  }

  if (!closest_distance_fragment)
    return CreatePositionWithAffinity(0);

  int offset = closest_distance_box->OffsetForPositionInFragment(
      *closest_distance_fragment,
      LayoutUnit(absolute_point.X() - closest_distance_position), true);
  return CreatePositionWithAffinity(
      offset + closest_distance_box->Start(),
      offset > 0 ? VP_UPSTREAM_IF_POSSIBLE : TextAffinity::kDownstream);
}

namespace {

inline bool IsValidSurrogatePair(const TextRun& run, unsigned index) {
  if (!U16_IS_LEAD(run[index]))
    return false;
  if (index + 1 >= run.length())
    return false;
  return U16_IS_TRAIL(run[index + 1]);
}

TextRun ConstructTextRun(LayoutSVGInlineText& text,
                         unsigned position,
                         unsigned length,
                         TextDirection text_direction) {
  const ComputedStyle& style = text.StyleRef();

  TextRun run(
      // characters, will be set below if non-zero.
      static_cast<const LChar*>(nullptr),
      0,  // length, will be set below if non-zero.
      0,  // xPos, only relevant with allowTabs=true
      0,  // padding, only relevant for justified text, not relevant for SVG
      TextRun::kAllowTrailingExpansion, text_direction,
      IsOverride(style.GetUnicodeBidi()) /* directionalOverride */);

  if (length) {
    if (text.Is8Bit())
      run.SetText(text.Characters8() + position, length);
    else
      run.SetText(text.Characters16() + position, length);
  }

  // We handle letter & word spacing ourselves.
  run.DisableSpacing();

  // Propagate the maximum length of the characters buffer to the TextRun, even
  // when we're only processing a substring.
  run.SetCharactersLength(text.TextLength() - position);
  DCHECK_GE(run.CharactersLength(), run.length());
  return run;
}

// TODO(pdr): We only have per-glyph data so we need to synthesize per-grapheme
// data. E.g., if 'fi' is shaped into a single glyph, we do not know the 'i'
// position. The code below synthesizes an average glyph width when characters
// share a single position. This will incorrectly split combining diacritics.
// See: https://crbug.com/473476.
void SynthesizeGraphemeWidths(const TextRun& run,
                              Vector<CharacterRange>& ranges) {
  unsigned distribute_count = 0;
  for (int range_index = static_cast<int>(ranges.size()) - 1; range_index >= 0;
       --range_index) {
    CharacterRange& current_range = ranges[range_index];
    if (current_range.Width() == 0) {
      distribute_count++;
    } else if (distribute_count != 0) {
      // Only count surrogate pairs as a single character.
      bool surrogate_pair = IsValidSurrogatePair(run, range_index);
      if (!surrogate_pair)
        distribute_count++;

      float new_width = current_range.Width() / distribute_count;
      current_range.end = current_range.start + new_width;
      float last_end_position = current_range.end;
      for (unsigned distribute = 1; distribute < distribute_count;
           distribute++) {
        // This surrogate pair check will skip processing of the second
        // character forming the surrogate pair.
        unsigned distribute_index =
            range_index + distribute + (surrogate_pair ? 1 : 0);
        ranges[distribute_index].start = last_end_position;
        ranges[distribute_index].end = last_end_position + new_width;
        last_end_position = ranges[distribute_index].end;
      }

      distribute_count = 0;
    }
  }
}

}  // namespace

void LayoutSVGInlineText::AddMetricsFromRun(
    const TextRun& run,
    bool& last_character_was_white_space) {
  Vector<CharacterRange> char_ranges =
      ScaledFont().IndividualCharacterRanges(run);
  SynthesizeGraphemeWidths(run, char_ranges);

  const SimpleFontData* font_data = ScaledFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  const float cached_font_height =
      font_data->GetFontMetrics().FloatHeight() / scaling_factor_;
  const bool preserve_white_space =
      StyleRef().WhiteSpace() == EWhiteSpace::kPre;
  const unsigned run_length = run.length();

  // TODO(pdr): Character-based iteration is ambiguous and error-prone. It
  // should be unified under a single concept. See: https://crbug.com/593570
  unsigned character_index = 0;
  while (character_index < run_length) {
    bool current_character_is_white_space = run[character_index] == ' ';
    if (!preserve_white_space && last_character_was_white_space &&
        current_character_is_white_space) {
      metrics_.push_back(SVGTextMetrics(SVGTextMetrics::kSkippedSpaceMetrics));
      character_index++;
      continue;
    }

    unsigned length = IsValidSurrogatePair(run, character_index) ? 2 : 1;
    float width = char_ranges[character_index].Width() / scaling_factor_;

    metrics_.push_back(SVGTextMetrics(length, width, cached_font_height));

    last_character_was_white_space = current_character_is_white_space;
    character_index += length;
  }
}

void LayoutSVGInlineText::UpdateMetricsList(
    bool& last_character_was_white_space) {
  metrics_.clear();

  if (!TextLength())
    return;

  TextRun run =
      ConstructTextRun(*this, 0, TextLength(), StyleRef().Direction());
  BidiResolver<TextRunIterator, BidiCharacterRun> bidi_resolver;
  BidiRunList<BidiCharacterRun>& bidi_runs = bidi_resolver.Runs();
  bool bidi_override = IsOverride(StyleRef().GetUnicodeBidi());
  BidiStatus status(TextDirection::kLtr, bidi_override);
  if (run.Is8Bit() || bidi_override) {
    WTF::Unicode::CharDirection direction = WTF::Unicode::kLeftToRight;
    // If BiDi override is in effect, use the specified direction.
    if (bidi_override && !StyleRef().IsLeftToRightDirection())
      direction = WTF::Unicode::kRightToLeft;
    bidi_runs.AddRun(new BidiCharacterRun(
        status.context->Override(), status.context->Level(), 0,
        run.CharactersLength(), direction, status.context->Dir()));
  } else {
    status.last = status.last_strong = WTF::Unicode::kOtherNeutral;
    bidi_resolver.SetStatus(status);
    bidi_resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
    const bool kHardLineBreak = false;
    const bool kReorderRuns = false;
    bidi_resolver.CreateBidiRunsForLine(TextRunIterator(&run, run.length()),
                                        kNoVisualOverride, kHardLineBreak,
                                        kReorderRuns);
  }

  for (const BidiCharacterRun* bidi_run = bidi_runs.FirstRun(); bidi_run;
       bidi_run = bidi_run->Next()) {
    TextRun sub_run = ConstructTextRun(*this, bidi_run->Start(),
                                       bidi_run->Stop() - bidi_run->Start(),
                                       bidi_run->Direction());
    AddMetricsFromRun(sub_run, last_character_was_white_space);
  }

  bidi_resolver.Runs().DeleteRuns();
}

void LayoutSVGInlineText::UpdateScaledFont() {
  ComputeNewScaledFontForStyle(*this, scaling_factor_, scaled_font_);
}

void LayoutSVGInlineText::ComputeNewScaledFontForStyle(
    const LayoutObject& layout_object,
    float& scaling_factor,
    Font& scaled_font) {
  const ComputedStyle& style = layout_object.StyleRef();

  // Alter font-size to the right on-screen value to avoid scaling the glyphs
  // themselves, except when GeometricPrecision is specified.
  scaling_factor =
      SVGLayoutSupport::CalculateScreenFontSizeScalingFactor(&layout_object);
  if (!scaling_factor) {
    scaling_factor = 1;
    scaled_font = style.GetFont();
    return;
  }

  const FontDescription& unscaled_font_description = style.GetFontDescription();
  if (unscaled_font_description.TextRendering() == kGeometricPrecision)
    scaling_factor = 1;

  Document& document = layout_object.GetDocument();
  float scaled_font_size = FontSize::GetComputedSizeFromSpecifiedSize(
      &document, scaling_factor, unscaled_font_description.IsAbsoluteSize(),
      unscaled_font_description.SpecifiedSize(), kDoNotApplyMinimumForFontSize);
  if (scaled_font_size == unscaled_font_description.ComputedSize()) {
    scaled_font = style.GetFont();
    return;
  }

  FontDescription font_description = unscaled_font_description;
  font_description.SetComputedSize(scaled_font_size);

  scaled_font = Font(font_description);
  scaled_font.Update(document.GetStyleEngine().FontSelector());
}

LayoutRect LayoutSVGInlineText::AbsoluteVisualRect() const {
  return Parent()->AbsoluteVisualRect();
}

FloatRect LayoutSVGInlineText::VisualRectInLocalSVGCoordinates() const {
  return Parent()->VisualRectInLocalSVGCoordinates();
}

RefPtr<StringImpl> LayoutSVGInlineText::OriginalText() const {
  RefPtr<StringImpl> result = LayoutText::OriginalText();
  if (!result)
    return nullptr;
  return NormalizeWhitespace(std::move(result));
}

}  // namespace blink
