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

#ifndef LayoutTextFragment_h
#define LayoutTextFragment_h

#include "core/editing/EditingUtilities.h"
#include "core/layout/LayoutText.h"
#include "platform/heap/Handle.h"

namespace blink {

class FirstLetterPseudoElement;

// Used to represent a text substring of an element, e.g., for text runs that
// are split because of first letter and that must therefore have different
// styles (and positions in the layout tree).
// We cache offsets so that text transformations can be applied in such a way
// that we can recover the original unaltered string from our corresponding DOM
// node.
class CORE_EXPORT LayoutTextFragment final : public LayoutText {
 public:
  LayoutTextFragment(Node*, StringImpl*, int start_offset, int length);
  LayoutTextFragment(Node*, StringImpl*);
  ~LayoutTextFragment() override;

  static LayoutTextFragment* CreateAnonymous(PseudoElement&, StringImpl*);
  static LayoutTextFragment* CreateAnonymous(PseudoElement&,
                                             StringImpl*,
                                             unsigned start,
                                             unsigned length);

  bool IsTextFragment() const override { return true; }

  bool ContainsCaretOffset(int) const override;
  int CaretMinOffset() const override;
  int CaretMaxOffset() const override;
  unsigned ResolvedTextLength() const override;

  unsigned Start() const { return start_; }
  unsigned FragmentLength() const { return fragment_length_; }

  unsigned TextStartOffset() const override { return Start(); }

  void SetContentString(StringImpl*);
  StringImpl* ContentString() const { return content_string_.get(); }
  // The complete text is all of the text in the associated DOM text node.
  scoped_refptr<StringImpl> CompleteText() const;
  // The fragment text is the text which will be used by this
  // LayoutTextFragment. For things like first-letter this may differ from the
  // completeText as we maybe using only a portion of the text nodes content.

  scoped_refptr<StringImpl> OriginalText() const override;

  void SetText(scoped_refptr<StringImpl>, bool force = false) override;
  void SetTextFragment(scoped_refptr<StringImpl>,
                       unsigned start,
                       unsigned length);

  void TransformText() override;

  // FIXME: Rename to LayoutTextFragment
  const char* GetName() const override { return "LayoutTextFragment"; }

  void SetFirstLetterPseudoElement(FirstLetterPseudoElement* element) {
    first_letter_pseudo_element_ = element;
  }
  FirstLetterPseudoElement* GetFirstLetterPseudoElement() const {
    return first_letter_pseudo_element_;
  }

  void SetIsRemainingTextLayoutObject(bool is_remaining_text) {
    is_remaining_text_layout_object_ = is_remaining_text;
  }
  bool IsRemainingTextLayoutObject() const {
    return is_remaining_text_layout_object_;
  }

  Text* AssociatedTextNode() const;

 protected:
  void WillBeDestroyed() override;

 private:
  LayoutBlock* BlockForAccompanyingFirstLetter() const;
  UChar PreviousCharacter() const override;

  void UpdateHitTestResult(HitTestResult&, const LayoutPoint&) override;

  unsigned start_;
  unsigned fragment_length_;
  bool is_remaining_text_layout_object_;
  scoped_refptr<StringImpl> content_string_;
  // Reference back to FirstLetterPseudoElement; cleared by
  // FirstLetterPseudoElement::detachLayoutTree() if it goes away first.
  UntracedMember<FirstLetterPseudoElement> first_letter_pseudo_element_;
};

DEFINE_TYPE_CASTS(LayoutTextFragment,
                  LayoutObject,
                  object,
                  ToLayoutText(object)->IsTextFragment(),
                  ToLayoutText(object).IsTextFragment());

}  // namespace blink

#endif  // LayoutTextFragment_h
