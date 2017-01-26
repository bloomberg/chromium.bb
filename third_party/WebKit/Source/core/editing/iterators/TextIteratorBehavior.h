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

  bool collapseTrailingSpace() const { return m_collapseTrailingSpace; }
  bool doesNotBreakAtReplacedElement() const {
    return m_doesNotBreakAtReplacedElement;
  }
  bool emitsCharactersBetweenAllVisiblePositions() const {
    return m_emitsCharactersBetweenAllVisiblePositions;
  }
  bool emitsImageAltText() const { return m_emitsImageAltText; }
  bool emitsSpaceForNbsp() const { return m_emitsSpaceForNbsp; }
  bool emitsObjectReplacementCharacter() const {
    return m_emitsObjectReplacementCharacter;
  }
  bool emitsOriginalText() const { return m_emitsOriginalText; }
  bool entersOpenShadowRoots() const { return m_entersOpenShadowRoots; }
  bool entersTextControls() const { return m_entersTextControls; }
  bool excludeAutofilledValue() const { return m_excludeAutofilledValue; }
  bool forInnerText() const { return m_forInnerText; }
  bool forSelectionToString() const { return m_forSelectionToString; }
  bool forWindowFind() const { return m_forWindowFind; }
  bool ignoresStyleVisibility() const { return m_ignoresStyleVisibility; }
  bool stopsOnFormControls() const { return m_stopsOnFormControls; }

  static TextIteratorBehavior emitsObjectReplacementCharacterBehavior();
  static TextIteratorBehavior ignoresStyleVisibilityBehavior();

 private:
  bool m_collapseTrailingSpace : 1;
  bool m_doesNotBreakAtReplacedElement : 1;
  bool m_emitsCharactersBetweenAllVisiblePositions : 1;
  bool m_emitsImageAltText : 1;
  bool m_emitsSpaceForNbsp : 1;
  bool m_emitsObjectReplacementCharacter : 1;
  bool m_emitsOriginalText : 1;
  bool m_entersOpenShadowRoots : 1;
  bool m_entersTextControls : 1;
  bool m_excludeAutofilledValue : 1;
  bool m_forInnerText : 1;
  bool m_forSelectionToString : 1;
  bool m_forWindowFind : 1;
  bool m_ignoresStyleVisibility : 1;
  bool m_stopsOnFormControls : 1;
};

class CORE_EXPORT TextIteratorBehavior::Builder final {
 public:
  explicit Builder(const TextIteratorBehavior&);
  Builder();
  ~Builder();

  TextIteratorBehavior build();

  Builder& setCollapseTrailingSpace(bool);
  Builder& setDoesNotBreakAtReplacedElement(bool);
  Builder& setEmitsCharactersBetweenAllVisiblePositions(bool);
  Builder& setEmitsImageAltText(bool);
  Builder& setEmitsSpaceForNbsp(bool);
  Builder& setEmitsObjectReplacementCharacter(bool);
  Builder& setEmitsOriginalText(bool);
  Builder& setEntersOpenShadowRoots(bool);
  Builder& setEntersTextControls(bool);
  Builder& setExcludeAutofilledValue(bool);
  Builder& setForInnerText(bool);
  Builder& setForSelectionToString(bool);
  Builder& setForWindowFind(bool);
  Builder& setIgnoresStyleVisibility(bool);
  Builder& setStopsOnFormControls(bool);

 private:
  TextIteratorBehavior m_behavior;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace blink

#endif  // TextIteratorBehavior_h
