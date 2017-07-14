/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGScriptElement.h"

#include "bindings/core/v8/HTMLScriptElementOrSVGScriptElement.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/XLinkNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/ScriptRunner.h"
#include "core/events/Event.h"
#include "core/frame/csp/ContentSecurityPolicy.h"

namespace blink {

inline SVGScriptElement::SVGScriptElement(Document& document,
                                          bool was_inserted_by_parser,
                                          bool already_started)
    : SVGElement(SVGNames::scriptTag, document),
      SVGURIReference(this),
      loader_(this,
              InitializeScriptLoader(was_inserted_by_parser,
                                     already_started,
                                     false)) {}

SVGScriptElement* SVGScriptElement::Create(Document& document,
                                           bool inserted_by_parser) {
  return new SVGScriptElement(document, inserted_by_parser, false);
}

void SVGScriptElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == HTMLNames::onerrorAttr) {
    SetAttributeEventListener(
        EventTypeNames::error,
        CreateAttributeEventListener(this, params.name, params.new_value,
                                     EventParameterName()));
  } else {
    SVGElement::ParseAttribute(params);
  }
}

void SVGScriptElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    loader_->HandleSourceAttribute(HrefString());
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

Node::InsertionNotificationRequest SVGScriptElement::InsertedInto(
    ContainerNode* root_parent) {
  SVGElement::InsertedInto(root_parent);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void SVGScriptElement::DidNotifySubtreeInsertionsToDocument() {
  loader_->DidNotifySubtreeInsertionsToDocument();

  if (!loader_->IsParserInserted())
    loader_->SetHaveFiredLoadEvent(true);
}

void SVGScriptElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);
  loader_->ChildrenChanged();
}

void SVGScriptElement::DidMoveToNewDocument(Document& old_document) {
  ScriptRunner::MovePendingScript(old_document, GetDocument(), loader_.Get());
  SVGElement::DidMoveToNewDocument(old_document);
}

bool SVGScriptElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == AtomicString(SourceAttributeValue());
}

void SVGScriptElement::FinishParsingChildren() {
  SVGElement::FinishParsingChildren();
  loader_->SetHaveFiredLoadEvent(true);
}

bool SVGScriptElement::HaveLoadedRequiredResources() {
  return loader_->HaveFiredLoadEvent();
}

String SVGScriptElement::SourceAttributeValue() const {
  return HrefString();
}

String SVGScriptElement::TypeAttributeValue() const {
  return getAttribute(SVGNames::typeAttr).GetString();
}

String SVGScriptElement::TextFromChildren() {
  return Element::TextFromChildren();
}

bool SVGScriptElement::HasSourceAttribute() const {
  return href()->IsSpecified();
}

bool SVGScriptElement::IsConnected() const {
  return Node::isConnected();
}

bool SVGScriptElement::HasChildren() const {
  return Node::hasChildren();
}

const AtomicString& SVGScriptElement::GetNonceForElement() const {
  return ContentSecurityPolicy::IsNonceableElement(this) ? nonce()
                                                         : g_null_atom;
}

bool SVGScriptElement::AllowInlineScriptForCSP(
    const AtomicString& nonce,
    const WTF::OrdinalNumber& context_line,
    const String& script_content,
    ContentSecurityPolicy::InlineType inline_type) {
  return GetDocument().GetContentSecurityPolicy()->AllowInlineScript(
      this, GetDocument().Url(), nonce, context_line, script_content,
      inline_type);
}

AtomicString SVGScriptElement::InitiatorName() const {
  return Element::localName();
}

Document& SVGScriptElement::GetDocument() const {
  return Node::GetDocument();
}

Element* SVGScriptElement::CloneElementWithoutAttributesAndChildren() {
  return new SVGScriptElement(GetDocument(), false, loader_->AlreadyStarted());
}

void SVGScriptElement::DispatchLoadEvent() {
  DispatchEvent(Event::Create(EventTypeNames::load));
}

void SVGScriptElement::DispatchErrorEvent() {
  DispatchEvent(Event::Create(EventTypeNames::error));
}

void SVGScriptElement::SetScriptElementForBinding(
    HTMLScriptElementOrSVGScriptElement& element) {
  if (!IsInV1ShadowTree())
    element.setSVGScriptElement(this);
}

#if DCHECK_IS_ON()
bool SVGScriptElement::IsAnimatableAttribute(const QualifiedName& name) const {
  if (name == SVGNames::typeAttr || name == SVGNames::hrefAttr ||
      name == XLinkNames::hrefAttr)
    return false;
  return SVGElement::IsAnimatableAttribute(name);
}
#endif

DEFINE_TRACE(SVGScriptElement) {
  visitor->Trace(loader_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  ScriptElementBase::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(SVGScriptElement) {
  visitor->TraceWrappers(loader_);
  SVGElement::TraceWrappers(visitor);
}

}  // namespace blink
