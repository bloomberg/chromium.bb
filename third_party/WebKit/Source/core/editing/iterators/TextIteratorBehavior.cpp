// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/TextIteratorBehavior.h"

namespace blink {

TextIteratorBehavior::Builder::Builder(const TextIteratorBehavior& behavior)
    : behavior_(behavior) {}

TextIteratorBehavior::Builder::Builder() = default;
TextIteratorBehavior::Builder::~Builder() = default;

TextIteratorBehavior TextIteratorBehavior::Builder::Build() {
  return behavior_;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetCollapseTrailingSpace(bool value) {
  behavior_.collapse_trailing_space_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetDoesNotBreakAtReplacedElement(bool value) {
  behavior_.does_not_break_at_replaced_element_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsCharactersBetweenAllVisiblePositions(
    bool value) {
  behavior_.emits_characters_between_all_visible_positions_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsImageAltText(bool value) {
  behavior_.emits_image_alt_text_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsSpaceForNbsp(bool value) {
  behavior_.emits_space_for_nbsp_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsObjectReplacementCharacter(bool value) {
  behavior_.emits_object_replacement_character_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsOriginalText(bool value) {
  behavior_.emits_original_text_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEntersOpenShadowRoots(bool value) {
  behavior_.enters_open_shadow_roots_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEntersTextControls(bool value) {
  behavior_.enters_text_controls_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetExcludeAutofilledValue(bool value) {
  behavior_.exclude_autofilled_value_ = value;
  return *this;
}

TextIteratorBehavior::Builder& TextIteratorBehavior::Builder::SetForInnerText(
    bool value) {
  behavior_.for_inner_text_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetForSelectionToString(bool value) {
  behavior_.for_selection_to_string_ = value;
  return *this;
}

TextIteratorBehavior::Builder& TextIteratorBehavior::Builder::SetForWindowFind(
    bool value) {
  behavior_.for_window_find_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetIgnoresStyleVisibility(bool value) {
  behavior_.ignores_style_visibility_ = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetStopsOnFormControls(bool value) {
  behavior_.stops_on_form_controls_ = value;
  return *this;
}

// -
TextIteratorBehavior::TextIteratorBehavior(const TextIteratorBehavior& other) =
    default;

TextIteratorBehavior::TextIteratorBehavior()
    : collapse_trailing_space_(false),
      does_not_break_at_replaced_element_(false),
      emits_characters_between_all_visible_positions_(false),
      emits_image_alt_text_(false),
      emits_space_for_nbsp_(false),
      emits_object_replacement_character_(false),
      emits_original_text_(false),
      enters_open_shadow_roots_(false),
      enters_text_controls_(false),
      exclude_autofilled_value_(false),
      for_inner_text_(false),
      for_selection_to_string_(false),
      for_window_find_(false),
      ignores_style_visibility_(false),
      stops_on_form_controls_(false) {}

TextIteratorBehavior::~TextIteratorBehavior() = default;

bool TextIteratorBehavior::operator==(const TextIteratorBehavior& other) const {
  return collapse_trailing_space_ == other.collapse_trailing_space_ ||
         does_not_break_at_replaced_element_ ==
             other.does_not_break_at_replaced_element_ ||
         emits_characters_between_all_visible_positions_ ==
             other.emits_characters_between_all_visible_positions_ ||
         emits_image_alt_text_ == other.emits_image_alt_text_ ||
         emits_space_for_nbsp_ == other.emits_space_for_nbsp_ ||
         emits_object_replacement_character_ ==
             other.emits_object_replacement_character_ ||
         emits_original_text_ == other.emits_original_text_ ||
         enters_open_shadow_roots_ == other.enters_open_shadow_roots_ ||
         enters_text_controls_ == other.enters_text_controls_ ||
         exclude_autofilled_value_ == other.exclude_autofilled_value_ ||
         for_inner_text_ == other.for_inner_text_ ||
         for_selection_to_string_ == other.for_selection_to_string_ ||
         for_window_find_ == other.for_window_find_ ||
         ignores_style_visibility_ == other.ignores_style_visibility_ ||
         stops_on_form_controls_ == other.stops_on_form_controls_;
}

bool TextIteratorBehavior::operator!=(const TextIteratorBehavior& other) const {
  return !operator==(other);
}

// static
TextIteratorBehavior
TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .Build();
}

// static
TextIteratorBehavior TextIteratorBehavior::IgnoresStyleVisibilityBehavior() {
  return TextIteratorBehavior::Builder()
      .SetIgnoresStyleVisibility(true)
      .Build();
}

}  // namespace blink
