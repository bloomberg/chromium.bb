/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "core/css/StyleSheetCandidate.h"

#include "core/HTMLNames.h"
#include "core/css/StyleEngine.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/imports/HTMLImport.h"
#include "core/svg/SVGStyleElement.h"

namespace blink {

using namespace HTMLNames;

AtomicString StyleSheetCandidate::Title() const {
  return IsElement() ? ToElement(GetNode()).FastGetAttribute(titleAttr)
                     : g_null_atom;
}

bool StyleSheetCandidate::IsXSL() const {
  return !GetNode().GetDocument().IsHTMLDocument() && type_ == kPi &&
         ToProcessingInstruction(GetNode()).IsXSL();
}

bool StyleSheetCandidate::IsImport() const {
  return type_ == kHTMLLink && toHTMLLinkElement(GetNode()).IsImport();
}

bool StyleSheetCandidate::IsCSSStyle() const {
  return type_ == kHTMLStyle || type_ == kSVGStyle;
}

Document* StyleSheetCandidate::ImportedDocument() const {
  DCHECK(IsImport());
  return toHTMLLinkElement(GetNode()).import();
}

bool StyleSheetCandidate::IsAlternate() const {
  if (!IsElement())
    return false;
  return ToElement(GetNode()).getAttribute(relAttr).Contains("alternate");
}

bool StyleSheetCandidate::IsEnabledViaScript() const {
  return IsHTMLLink() && toHTMLLinkElement(GetNode()).IsEnabledViaScript();
}

bool StyleSheetCandidate::IsEnabledAndLoading() const {
  return IsHTMLLink() && !toHTMLLinkElement(GetNode()).IsDisabled() &&
         toHTMLLinkElement(GetNode()).StyleSheetIsLoading();
}

bool StyleSheetCandidate::CanBeActivated(
    const String& current_preferrable_name) const {
  StyleSheet* sheet = this->Sheet();
  if (!sheet || sheet->disabled() || !sheet->IsCSSStyleSheet())
    return false;

  if (sheet->ownerNode() && sheet->ownerNode()->IsInShadowTree()) {
    if (IsCSSStyle())
      return true;
    if (IsHTMLLink() && !IsImport())
      return !IsAlternate();
  }

  const AtomicString& title = this->Title();
  if (!IsEnabledViaScript() && !title.IsEmpty() &&
      title != current_preferrable_name)
    return false;
  if (IsAlternate() && title.IsEmpty())
    return false;

  return true;
}

StyleSheetCandidate::Type StyleSheetCandidate::TypeOf(Node& node) {
  if (node.getNodeType() == Node::kProcessingInstructionNode)
    return kPi;

  if (node.IsHTMLElement()) {
    if (isHTMLLinkElement(node))
      return kHTMLLink;
    if (isHTMLStyleElement(node))
      return kHTMLStyle;

    NOTREACHED();
    return kInvalid;
  }

  if (isSVGStyleElement(node))
    return kSVGStyle;

  NOTREACHED();
  return kInvalid;
}

StyleSheet* StyleSheetCandidate::Sheet() const {
  switch (type_) {
    case kHTMLLink:
      return toHTMLLinkElement(GetNode()).sheet();
    case kHTMLStyle:
      return toHTMLStyleElement(GetNode()).sheet();
    case kSVGStyle:
      return toSVGStyleElement(GetNode()).sheet();
    case kPi:
      return ToProcessingInstruction(GetNode()).sheet();
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
