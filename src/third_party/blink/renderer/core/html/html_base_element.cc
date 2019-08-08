/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/html/html_base_element.h"

#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"

namespace blink {

using namespace html_names;

HTMLBaseElement::HTMLBaseElement(Document& document)
    : HTMLElement(kBaseTag, document) {}

const AttrNameToTrustedType& HTMLBaseElement::GetCheckedAttributeTypes() const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({{"href", SpecificTrustedType::kTrustedURL}}));
  return attribute_map;
}

void HTMLBaseElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == kHrefAttr || params.name == kTargetAttr)
    GetDocument().ProcessBaseElement();
  else
    HTMLElement::ParseAttribute(params);
}

Node::InsertionNotificationRequest HTMLBaseElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (insertion_point.isConnected())
    GetDocument().ProcessBaseElement();
  return kInsertionDone;
}

void HTMLBaseElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (insertion_point.isConnected())
    GetDocument().ProcessBaseElement();
}

bool HTMLBaseElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == kHrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLBaseElement::href(USVStringOrTrustedURL& result) const {
  result.SetUSVString(href());
}

KURL HTMLBaseElement::href() const {
  // This does not use the GetURLAttribute function because that will resolve
  // relative to the document's base URL; base elements like this one can be
  // used to set that base URL. Thus we need to resolve relative to the
  // document's fallback base URL and ignore the base URL.
  // https://html.spec.whatwg.org/C/#dom-base-href

  const AtomicString& attribute_value = FastGetAttribute(kHrefAttr);
  if (attribute_value.IsNull())
    return GetDocument().Url();

  KURL url = GetDocument().Encoding().IsValid()
                 ? KURL(GetDocument().FallbackBaseURL(),
                        StripLeadingAndTrailingHTMLSpaces(attribute_value))
                 : KURL(GetDocument().FallbackBaseURL(),
                        StripLeadingAndTrailingHTMLSpaces(attribute_value),
                        GetDocument().Encoding());

  if (!url.IsValid())
    return KURL();

  return url;
}

void HTMLBaseElement::setHref(const USVStringOrTrustedURL& stringOrUrl,
                              ExceptionState& exception_state) {
  setAttribute(kHrefAttr, stringOrUrl, exception_state);
}

}  // namespace blink
