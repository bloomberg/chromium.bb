// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextIteratorBehavior_h
#define TextIteratorBehavior_h

#include "base/macros.h"
#include "core/CoreExport.h"

namespace blink {

class CORE_EXPORT TextIteratorBehavior final {
 public:
  class CORE_EXPORT Builder;

  TextIteratorBehavior(const TextIteratorBehavior& other);
  TextIteratorBehavior();
  ~TextIteratorBehavior();

  bool operator==(const TextIteratorBehavior& other) const;
  bool operator!=(const TextIteratorBehavior& other) const;

  bool CollapseTrailingSpace() const { return collapse_trailing_space_; }
  bool DoesNotBreakAtReplacedElement() const {
    return does_not_break_at_replaced_element_;
  }
  bool EmitsCharactersBetweenAllVisiblePositions() const {
    return emits_characters_between_all_visible_positions_;
  }
  bool EmitsImageAltText() const { return emits_image_alt_text_; }
  bool EmitsSpaceForNbsp() const { return emits_space_for_nbsp_; }
  bool EmitsObjectReplacementCharacter() const {
    return emits_object_replacement_character_;
  }
  bool EmitsOriginalText() const { return emits_original_text_; }
  bool EntersOpenShadowRoots() const { return enters_open_shadow_roots_; }
  bool EntersTextControls() const { return enters_text_controls_; }
  bool ExcludeAutofilledValue() const { return exclude_autofilled_value_; }
  bool ForInnerText() const { return for_inner_text_; }
  bool ForSelectionToString() const { return for_selection_to_string_; }
  bool ForWindowFind() const { return for_window_find_; }
  bool IgnoresStyleVisibility() const { return ignores_style_visibility_; }
  bool StopsOnFormControls() const { return stops_on_form_controls_; }

  static TextIteratorBehavior EmitsObjectReplacementCharacterBehavior();
  static TextIteratorBehavior IgnoresStyleVisibilityBehavior();

 private:
  bool collapse_trailing_space_ : 1;
  bool does_not_break_at_replaced_element_ : 1;
  bool emits_characters_between_all_visible_positions_ : 1;
  bool emits_image_alt_text_ : 1;
  bool emits_space_for_nbsp_ : 1;
  bool emits_object_replacement_character_ : 1;
  bool emits_original_text_ : 1;
  bool enters_open_shadow_roots_ : 1;
  bool enters_text_controls_ : 1;
  bool exclude_autofilled_value_ : 1;
  bool for_inner_text_ : 1;
  bool for_selection_to_string_ : 1;
  bool for_window_find_ : 1;
  bool ignores_style_visibility_ : 1;
  bool stops_on_form_controls_ : 1;
};

class CORE_EXPORT TextIteratorBehavior::Builder final {
 public:
  explicit Builder(const TextIteratorBehavior&);
  Builder();
  ~Builder();

  TextIteratorBehavior Build();

  Builder& SetCollapseTrailingSpace(bool);
  Builder& SetDoesNotBreakAtReplacedElement(bool);
  Builder& SetEmitsCharactersBetweenAllVisiblePositions(bool);
  Builder& SetEmitsImageAltText(bool);
  Builder& SetEmitsSpaceForNbsp(bool);
  Builder& SetEmitsObjectReplacementCharacter(bool);
  Builder& SetEmitsOriginalText(bool);
  Builder& SetEntersOpenShadowRoots(bool);
  Builder& SetEntersTextControls(bool);
  Builder& SetExcludeAutofilledValue(bool);
  Builder& SetForInnerText(bool);
  Builder& SetForSelectionToString(bool);
  Builder& SetForWindowFind(bool);
  Builder& SetIgnoresStyleVisibility(bool);
  Builder& SetStopsOnFormControls(bool);

 private:
  TextIteratorBehavior behavior_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace blink

#endif  // TextIteratorBehavior_h
