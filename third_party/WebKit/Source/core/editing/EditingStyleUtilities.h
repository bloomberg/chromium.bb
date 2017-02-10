/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EditingStyleUtilities_h
#define EditingStyleUtilities_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/WritingDirection.h"

namespace blink {

class CSSStyleDeclaration;
class EditingStyle;
class MutableStylePropertySet;
class Node;
class StylePropertySet;

class EditingStyleUtilities {
  STATIC_ONLY(EditingStyleUtilities);

 public:
  static EditingStyle* createWrappingStyleForAnnotatedSerialization(
      ContainerNode* context);
  static EditingStyle* createWrappingStyleForSerialization(
      ContainerNode* context);
  static EditingStyle* createStyleAtSelectionStart(
      const VisibleSelection&,
      bool shouldUseBackgroundColorInEffect = false,
      MutableStylePropertySet* styleToCheck = nullptr);
  static WritingDirection textDirectionForSelection(
      const VisibleSelection&,
      EditingStyle* typingStyle,
      bool& hasNestedOrMultipleEmbeddings);
  static bool isEmbedOrIsolate(CSSValueID unicodeBidi) {
    return unicodeBidi == CSSValueIsolate ||
           unicodeBidi == CSSValueWebkitIsolate || unicodeBidi == CSSValueEmbed;
  }

  static bool isTransparentColorValue(const CSSValue*);
  static bool hasTransparentBackgroundColor(CSSStyleDeclaration*);
  static bool hasTransparentBackgroundColor(StylePropertySet*);
  static const CSSValue* backgroundColorValueInEffect(Node*);
  static bool hasAncestorVerticalAlignStyle(Node&, CSSValueID);
};

}  // namespace blink

#endif  // EditingStyleUtilities_h
