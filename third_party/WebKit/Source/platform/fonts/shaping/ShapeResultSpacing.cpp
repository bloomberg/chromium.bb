// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultSpacing.h"

#include "platform/fonts/FontDescription.h"
#include "platform/text/TextRun.h"

namespace blink {

template <typename TextContainerType>
ShapeResultSpacing<TextContainerType>::ShapeResultSpacing(
    const TextContainerType& text)
    : text_(text),
      letter_spacing_(0),
      word_spacing_(0),
      expansion_(0),
      expansion_per_opportunity_(0),
      expansion_opportunity_count_(0),
      text_justify_(TextJustify::kAuto),
      has_spacing_(false),
      normalize_space_(false),
      allow_tabs_(false),
      is_after_expansion_(false),
      is_vertical_offset_(false) {}

template <typename TextContainerType>
bool ShapeResultSpacing<TextContainerType>::SetSpacing(
    const FontDescription& font_description) {
  if (!font_description.LetterSpacing() && !font_description.WordSpacing()) {
    has_spacing_ = false;
    return false;
  }

  letter_spacing_ = font_description.LetterSpacing();
  word_spacing_ = font_description.WordSpacing();
  is_vertical_offset_ = font_description.IsVerticalAnyUpright();
  DCHECK(!normalize_space_);
  allow_tabs_ = true;
  has_spacing_ = true;
  return true;
}

template <typename TextContainerType>
void ShapeResultSpacing<TextContainerType>::SetSpacingAndExpansion(
    const FontDescription& font_description) {
  // Available only for TextRun since it has expansion data.
  NOTREACHED();
}

template <>
void ShapeResultSpacing<TextRun>::SetSpacingAndExpansion(
    const FontDescription& font_description) {
  letter_spacing_ = font_description.LetterSpacing();
  word_spacing_ = font_description.WordSpacing();
  expansion_ = text_.Expansion();
  has_spacing_ = letter_spacing_ || word_spacing_ || expansion_;
  if (!has_spacing_)
    return;

  is_vertical_offset_ = font_description.IsVerticalAnyUpright();
  normalize_space_ = text_.NormalizeSpace();
  allow_tabs_ = text_.AllowTabs();

  if (expansion_) {
    ComputeExpansion(text_.AllowsLeadingExpansion(),
                     text_.AllowsTrailingExpansion(), text_.Direction(),
                     text_.GetTextJustify());
  }
}

template <typename TextContainerType>
void ShapeResultSpacing<TextContainerType>::ComputeExpansion(
    bool allows_leading_expansion,
    bool allows_trailing_expansion,
    TextDirection direction,
    TextJustify text_justify) {
  DCHECK_GT(expansion_, 0);

  text_justify_ = text_justify;
  is_after_expansion_ = !allows_leading_expansion;

  bool is_after_expansion = is_after_expansion_;
  if (text_.Is8Bit()) {
    expansion_opportunity_count_ = Character::ExpansionOpportunityCount(
        text_.Characters8(), text_.length(), direction, is_after_expansion,
        text_justify_);
  } else {
    expansion_opportunity_count_ = Character::ExpansionOpportunityCount(
        text_.Characters16(), text_.length(), direction, is_after_expansion,
        text_justify_);
  }
  if (is_after_expansion && !allows_trailing_expansion) {
    DCHECK_GT(expansion_opportunity_count_, 0u);
    --expansion_opportunity_count_;
  }

  if (expansion_opportunity_count_)
    expansion_per_opportunity_ = expansion_ / expansion_opportunity_count_;
}

template <typename TextContainerType>
float ShapeResultSpacing<TextContainerType>::NextExpansion() {
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

// Test if the |run| is the first sub-run of the original text container, for
// containers that can create sub-runs such as TextRun or StringView.
template <typename TextContainerType>
inline bool ShapeResultSpacing<TextContainerType>::IsFirstRun(
    const TextContainerType& run) const {
  return &run == &text_ || run.Bytes() == text_.Bytes();
}

template <>
inline bool ShapeResultSpacing<String>::IsFirstRun(const String& run) const {
  // String::Substring() should not be used because it copies to a new buffer.
  return &run == &text_ || run.Impl() == text_.Impl();
}

template <typename TextContainerType>
float ShapeResultSpacing<TextContainerType>::ComputeSpacing(
    const TextContainerType& run,
    size_t index,
    float& offset) {
  DCHECK(has_spacing_);
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

// Instantiate the template class.
template class ShapeResultSpacing<TextRun>;
template class ShapeResultSpacing<String>;

}  // namespace blink
