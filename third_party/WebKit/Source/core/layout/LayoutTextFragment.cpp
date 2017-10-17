/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#include "core/layout/LayoutTextFragment.h"

#include "core/css/StyleChangeReason.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/Text.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/ng/inline/ng_offset_mapping_result.h"

namespace blink {

LayoutTextFragment::LayoutTextFragment(Node* node,
                                       StringImpl* str,
                                       int start_offset,
                                       int length)
    : LayoutText(node, str ? str->Substring(start_offset, length) : nullptr),
      start_(start_offset),
      fragment_length_(length),
      is_remaining_text_layout_object_(false),
      content_string_(str),
      first_letter_pseudo_element_(nullptr) {}

LayoutTextFragment::LayoutTextFragment(Node* node, StringImpl* str)
    : LayoutTextFragment(node, str, 0, str ? str->length() : 0) {}

LayoutTextFragment::~LayoutTextFragment() {
  DCHECK(!first_letter_pseudo_element_);
}

LayoutTextFragment* LayoutTextFragment::CreateAnonymous(PseudoElement& pseudo,
                                                        StringImpl* text,
                                                        unsigned start,
                                                        unsigned length) {
  LayoutTextFragment* fragment =
      new LayoutTextFragment(nullptr, text, start, length);
  fragment->SetDocumentForAnonymous(&pseudo.GetDocument());
  if (length)
    pseudo.GetDocument().View()->IncrementVisuallyNonEmptyCharacterCount(
        length);
  return fragment;
}

LayoutTextFragment* LayoutTextFragment::CreateAnonymous(PseudoElement& pseudo,
                                                        StringImpl* text) {
  return CreateAnonymous(pseudo, text, 0, text ? text->length() : 0);
}

void LayoutTextFragment::WillBeDestroyed() {
  if (is_remaining_text_layout_object_ && first_letter_pseudo_element_)
    first_letter_pseudo_element_->SetRemainingTextLayoutObject(nullptr);
  first_letter_pseudo_element_ = nullptr;
  LayoutText::WillBeDestroyed();
}

RefPtr<StringImpl> LayoutTextFragment::CompleteText() const {
  Text* text = AssociatedTextNode();
  return text ? text->DataImpl() : ContentString();
}

void LayoutTextFragment::SetContentString(StringImpl* str) {
  content_string_ = str;
  SetText(str);
}

RefPtr<StringImpl> LayoutTextFragment::OriginalText() const {
  RefPtr<StringImpl> result = CompleteText();
  if (!result)
    return nullptr;
  return result->Substring(Start(), FragmentLength());
}

void LayoutTextFragment::SetText(RefPtr<StringImpl> text, bool force) {
  LayoutText::SetText(std::move(text), force);

  start_ = 0;
  fragment_length_ = TextLength();

  // If we're the remaining text from a first letter then we have to tell the
  // first letter pseudo element to reattach itself so it can re-calculate the
  // correct first-letter settings.
  if (IsRemainingTextLayoutObject()) {
    DCHECK(GetFirstLetterPseudoElement());
    GetFirstLetterPseudoElement()->UpdateTextFragments();
  }
}

void LayoutTextFragment::SetTextFragment(RefPtr<StringImpl> text,
                                         unsigned start,
                                         unsigned length) {
  LayoutText::SetText(std::move(text), false);

  start_ = start;
  fragment_length_ = length;
}

void LayoutTextFragment::TransformText() {
  // Note, we have to call LayoutText::setText here because, if we use our
  // version we will, potentially, screw up the first-letter settings where
  // we only use portions of the string.
  if (RefPtr<StringImpl> text_to_transform = OriginalText())
    LayoutText::SetText(std::move(text_to_transform), true);
}

UChar LayoutTextFragment::PreviousCharacter() const {
  if (Start()) {
    StringImpl* original = CompleteText().get();
    if (original && Start() <= original->length())
      return (*original)[Start() - 1];
  }

  return LayoutText::PreviousCharacter();
}

// If this is the layoutObject for a first-letter pseudoNode then we have to
// look at the node for the remaining text to find our content.
Text* LayoutTextFragment::AssociatedTextNode() const {
  Node* node = this->GetFirstLetterPseudoElement();
  if (is_remaining_text_layout_object_ || !node) {
    // If we don't have a node, then we aren't part of a first-letter pseudo
    // element, so use the actual node. Likewise, if we have a node, but
    // we're the remainingTextLayoutObject for a pseudo element use the real
    // text node.
    node = this->GetNode();
  }

  if (!node)
    return nullptr;

  if (node->IsFirstLetterPseudoElement()) {
    FirstLetterPseudoElement* pseudo = ToFirstLetterPseudoElement(node);
    LayoutObject* next_layout_object =
        FirstLetterPseudoElement::FirstLetterTextLayoutObject(*pseudo);
    if (!next_layout_object)
      return nullptr;
    node = next_layout_object->GetNode();
  }
  return (node && node->IsTextNode()) ? ToText(node) : nullptr;
}

void LayoutTextFragment::UpdateHitTestResult(HitTestResult& result,
                                             const LayoutPoint& point) {
  if (result.InnerNode())
    return;

  LayoutObject::UpdateHitTestResult(result, point);

  // If we aren't part of a first-letter element, or if we
  // are part of first-letter but we're the remaining text then return.
  if (is_remaining_text_layout_object_ || !GetFirstLetterPseudoElement())
    return;
  result.SetInnerNode(GetFirstLetterPseudoElement());
}

int LayoutTextFragment::CaretMinOffset() const {
  if (!ShouldUseNGAlternatives())
    return LayoutText::CaretMinOffset();

  const Node* node = AssociatedTextNode();
  if (!node)
    return 0;

  const unsigned candidate =
      GetNGOffsetMapping().StartOfNextNonCollapsedCharacter(*node, Start());
  DCHECK_GE(candidate, Start());
  // Align with the legacy behavior that 0 is returned if the entire layout
  // object contains only collapsed whitespaces.
  const unsigned adjusted = candidate - Start();
  return adjusted == FragmentLength() ? 0 : adjusted;
}

int LayoutTextFragment::CaretMaxOffset() const {
  if (!ShouldUseNGAlternatives())
    return LayoutText::CaretMaxOffset();

  const Node* node = AssociatedTextNode();
  if (!node)
    return 0;

  const unsigned candidate =
      GetNGOffsetMapping().EndOfLastNonCollapsedCharacter(
          *node, Start() + FragmentLength());
  // Align with the legacy behavior that FragmentLength() is returned if the
  // entire layout object contains only collapsed whitespaces.
  return candidate <= Start() ? FragmentLength() : candidate - Start();
}

unsigned LayoutTextFragment::ResolvedTextLength() const {
  if (!ShouldUseNGAlternatives())
    return LayoutText::ResolvedTextLength();

  const Node* node = AssociatedTextNode();
  if (!node)
    return 0;
  const NGOffsetMappingResult& mapping = GetNGOffsetMapping();
  const unsigned start = mapping.GetTextContentOffset(*node, Start());
  const unsigned end =
      mapping.GetTextContentOffset(*node, Start() + FragmentLength());
  DCHECK_LE(start, end);
  return end - start;
}

}  // namespace blink
