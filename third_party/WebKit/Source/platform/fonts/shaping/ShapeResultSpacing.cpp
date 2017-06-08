// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultSpacing.h"

#include "platform/fonts/FontDescription.h"
#include "platform/text/TextRun.h"

namespace blink {

ShapeResultSpacing::ShapeResultSpacing(const TextRun& run,
                                       const FontDescription& font_description)
    : text_run_(run),
      letter_spacing_(font_description.LetterSpacing()),
      word_spacing_(font_description.WordSpacing()),
      expansion_(run.Expansion()),
      expansion_per_opportunity_(0),
      expansion_opportunity_count_(0),
      text_justify_(TextJustify::kAuto),
      has_spacing_(false),
      normalize_space_(run.NormalizeSpace()),
      allow_tabs_(run.AllowTabs()),
      is_after_expansion_(false),
      is_vertical_offset_(font_description.IsVerticalAnyUpright()) {
  if (text_run_.SpacingDisabled())
    return;

  if (!letter_spacing_ && !word_spacing_ && !expansion_)
    return;

  has_spacing_ = true;

  if (!expansion_)
    return;

  // Setup for justifications (expansions.)
  text_justify_ = run.GetTextJustify();
  is_after_expansion_ = !run.AllowsLeadingExpansion();

  bool is_after_expansion = is_after_expansion_;
  expansion_opportunity_count_ =
      Character::ExpansionOpportunityCount(run, is_after_expansion);
  if (is_after_expansion && !run.AllowsTrailingExpansion()) {
    DCHECK_GT(expansion_opportunity_count_, 0u);
    --expansion_opportunity_count_;
  }

  if (expansion_opportunity_count_)
    expansion_per_opportunity_ = expansion_ / expansion_opportunity_count_;
}

float ShapeResultSpacing::NextExpansion() {
  if (!expansion_opportunity_count_) {
    NOTREACHED();
    return 0;
  }

  is_after_expansion_ = true;

  if (!--expansion_opportunity_count_) {
    float remaining = expansion_;
    expansion_ = 0;
    return remaining;
  }

  expansion_ -= expansion_per_opportunity_;
  return expansion_per_opportunity_;
}

bool ShapeResultSpacing::IsFirstRun(const TextRun& run) const {
  if (&run == &text_run_)
    return true;
  return run.Is8Bit() ? run.Characters8() == text_run_.Characters8()
                      : run.Characters16() == text_run_.Characters16();
}

float ShapeResultSpacing::ComputeSpacing(const TextRun& run,
                                         size_t index,
                                         float& offset) {
  UChar32 character = run[index];
  bool treat_as_space =
      (Character::TreatAsSpace(character) ||
       (normalize_space_ &&
        Character::IsNormalizedCanvasSpaceCharacter(character))) &&
      (character != '\t' || !allow_tabs_);
  if (treat_as_space && character != kNoBreakSpaceCharacter)
    character = kSpaceCharacter;

  float spacing = 0;
  if (letter_spacing_ && !Character::TreatAsZeroWidthSpace(character))
    spacing += letter_spacing_;

  if (treat_as_space &&
      (index || !IsFirstRun(run) || character == kNoBreakSpaceCharacter))
    spacing += word_spacing_;

  if (!HasExpansion())
    return spacing;

  if (treat_as_space)
    return spacing + NextExpansion();

  if (run.Is8Bit() || text_justify_ != TextJustify::kAuto)
    return spacing;

  // isCJKIdeographOrSymbol() has expansion opportunities both before and
  // after each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (U16_IS_LEAD(character) && index + 1 < run.length() &&
      U16_IS_TRAIL(run[index + 1]))
    character = U16_GET_SUPPLEMENTARY(character, run[index + 1]);
  if (!Character::IsCJKIdeographOrSymbol(character)) {
    is_after_expansion_ = false;
    return spacing;
  }

  if (!is_after_expansion_) {
    // Take the expansion opportunity before this ideograph.
    float expand_before = NextExpansion();
    if (expand_before) {
      offset += expand_before;
      spacing += expand_before;
    }
    if (!HasExpansion())
      return spacing;
  }

  return spacing + NextExpansion();
}

}  // namespace blink
