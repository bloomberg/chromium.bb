// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/TextIteratorBehavior.h"

namespace blink {

TextIteratorBehavior::Builder::Builder(const TextIteratorBehavior& behavior)
    : m_behavior(behavior) {}

TextIteratorBehavior::Builder::Builder() = default;
TextIteratorBehavior::Builder::~Builder() = default;

TextIteratorBehavior TextIteratorBehavior::Builder::build() {
  return m_behavior;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setCollapseTrailingSpace(bool value) {
  m_behavior.m_collapseTrailingSpace = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setDoesNotBreakAtReplacedElement(bool value) {
  m_behavior.m_doesNotBreakAtReplacedElement = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEmitsCharactersBetweenAllVisiblePositions(
    bool value) {
  m_behavior.m_emitsCharactersBetweenAllVisiblePositions = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEmitsImageAltText(bool value) {
  m_behavior.m_emitsImageAltText = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEmitsSpaceForNbsp(bool value) {
  m_behavior.m_emitsSpaceForNbsp = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEmitsObjectReplacementCharacter(bool value) {
  m_behavior.m_emitsObjectReplacementCharacter = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEmitsOriginalText(bool value) {
  m_behavior.m_emitsOriginalText = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEntersOpenShadowRoots(bool value) {
  m_behavior.m_entersOpenShadowRoots = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setEntersTextControls(bool value) {
  m_behavior.m_entersTextControls = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setExcludeAutofilledValue(bool value) {
  m_behavior.m_excludeAutofilledValue = value;
  return *this;
}

TextIteratorBehavior::Builder& TextIteratorBehavior::Builder::setForInnerText(
    bool value) {
  m_behavior.m_forInnerText = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setForSelectionToString(bool value) {
  m_behavior.m_forSelectionToString = value;
  return *this;
}

TextIteratorBehavior::Builder& TextIteratorBehavior::Builder::setForWindowFind(
    bool value) {
  m_behavior.m_forWindowFind = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setIgnoresStyleVisibility(bool value) {
  m_behavior.m_ignoresStyleVisibility = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::setStopsOnFormControls(bool value) {
  m_behavior.m_stopsOnFormControls = value;
  return *this;
}

// -
TextIteratorBehavior::TextIteratorBehavior(const TextIteratorBehavior& other) =
    default;

TextIteratorBehavior::TextIteratorBehavior()
    : m_collapseTrailingSpace(false),
      m_doesNotBreakAtReplacedElement(false),
      m_emitsCharactersBetweenAllVisiblePositions(false),
      m_emitsImageAltText(false),
      m_emitsSpaceForNbsp(false),
      m_emitsObjectReplacementCharacter(false),
      m_emitsOriginalText(false),
      m_entersOpenShadowRoots(false),
      m_entersTextControls(false),
      m_excludeAutofilledValue(false),
      m_forInnerText(false),
      m_forSelectionToString(false),
      m_forWindowFind(false),
      m_ignoresStyleVisibility(false),
      m_stopsOnFormControls(false) {}

TextIteratorBehavior::~TextIteratorBehavior() = default;

bool TextIteratorBehavior::operator==(const TextIteratorBehavior& other) const {
  return m_collapseTrailingSpace == other.m_collapseTrailingSpace ||
         m_doesNotBreakAtReplacedElement ==
             other.m_doesNotBreakAtReplacedElement ||
         m_emitsCharactersBetweenAllVisiblePositions ==
             other.m_emitsCharactersBetweenAllVisiblePositions ||
         m_emitsImageAltText == other.m_emitsImageAltText ||
         m_emitsSpaceForNbsp == other.m_emitsSpaceForNbsp ||
         m_emitsObjectReplacementCharacter ==
             other.m_emitsObjectReplacementCharacter ||
         m_emitsOriginalText == other.m_emitsOriginalText ||
         m_entersOpenShadowRoots == other.m_entersOpenShadowRoots ||
         m_entersTextControls == other.m_entersTextControls ||
         m_excludeAutofilledValue == other.m_excludeAutofilledValue ||
         m_forInnerText == other.m_forInnerText ||
         m_forSelectionToString == other.m_forSelectionToString ||
         m_forWindowFind == other.m_forWindowFind ||
         m_ignoresStyleVisibility == other.m_ignoresStyleVisibility ||
         m_stopsOnFormControls == other.m_stopsOnFormControls;
}

bool TextIteratorBehavior::operator!=(const TextIteratorBehavior& other) const {
  return !operator==(other);
}

// static
TextIteratorBehavior
TextIteratorBehavior::emitsObjectReplacementCharacterBehavior() {
  return TextIteratorBehavior::Builder()
      .setEmitsObjectReplacementCharacter(true)
      .build();
}

// static
TextIteratorBehavior TextIteratorBehavior::ignoresStyleVisibilityBehavior() {
  return TextIteratorBehavior::Builder()
      .setIgnoresStyleVisibility(true)
      .build();
}

}  // namespace blink
