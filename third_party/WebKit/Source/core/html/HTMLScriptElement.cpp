/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
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

#include "core/html/HTMLScriptElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/html_script_element_or_svg_script_element.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/ScriptRunner.h"
#include "core/dom/Text.h"
#include "core/dom/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html_names.h"

namespace blink {

using namespace HTMLNames;

inline HTMLScriptElement::HTMLScriptElement(Document& document,
                                            bool was_inserted_by_parser,
                                            bool already_started,
                                            bool created_during_document_write)
    : HTMLElement(scriptTag, document),
      loader_(InitializeScriptLoader(was_inserted_by_parser,
                                     already_started,
                                     created_during_document_write)) {}

HTMLScriptElement* HTMLScriptElement::Create(
    Document& document,
    bool was_inserted_by_parser,
    bool already_started,
    bool created_during_document_write) {
  return new HTMLScriptElement(document, was_inserted_by_parser,
                               already_started, created_during_document_write);
}

bool HTMLScriptElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == srcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLScriptElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == srcAttr || HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLScriptElement::SubResourceAttributeName() const {
  return srcAttr;
}

void HTMLScriptElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (change.IsChildInsertion())
    loader_->ChildrenChanged();
}

void HTMLScriptElement::DidMoveToNewDocument(Document& old_document) {
  ScriptRunner::MovePendingScript(old_document, GetDocument(), loader_.Get());
  HTMLElement::DidMoveToNewDocument(old_document);
}

void HTMLScriptElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == srcAttr) {
    loader_->HandleSourceAttribute(params.new_value);
    LogUpdateAttributeIfIsolatedWorldAndInDocument("script", params);
  } else if (params.name == asyncAttr) {
    loader_->HandleAsyncAttribute();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

Node::InsertionNotificationRequest HTMLScriptElement::InsertedInto(
    ContainerNode* insertion_point) {
  ScriptType script_type = ScriptType::kClassic;
  if (insertion_point->isConnected() && HasSourceAttribute() &&
      !Loader()->IsScriptTypeSupported(
          ScriptLoader::kDisallowLegacyTypeInTypeAttribute, script_type)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kScriptElementWithInvalidTypeHasSrc);
  }
  HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("script", srcAttr);

  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLScriptElement::DidNotifySubtreeInsertionsToDocument() {
  loader_->DidNotifySubtreeInsertionsToDocument();
}

void HTMLScriptElement::setText(const String& value) {
  setTextContent(value);
}

void HTMLScriptElement::setAsync(bool async) {
  SetBooleanAttribute(asyncAttr, async);
  loader_->HandleAsyncAttribute();
}

bool HTMLScriptElement::async() const {
  return FastHasAttribute(asyncAttr) || loader_->IsNonBlocking();
}

KURL HTMLScriptElement::Src() const {
  return GetDocument().CompleteURL(SourceAttributeValue());
}

String HTMLScriptElement::SourceAttributeValue() const {
  return getAttribute(srcAttr).GetString();
}

String HTMLScriptElement::CharsetAttributeValue() const {
  return getAttribute(charsetAttr).GetString();
}

String HTMLScriptElement::TypeAttributeValue() const {
  return getAttribute(typeAttr).GetString();
}

String HTMLScriptElement::LanguageAttributeValue() const {
  return getAttribute(languageAttr).GetString();
}

bool HTMLScriptElement::NomoduleAttributeValue() const {
  return FastHasAttribute(nomoduleAttr);
}

String HTMLScriptElement::ForAttributeValue() const {
  return getAttribute(forAttr).GetString();
}

String HTMLScriptElement::EventAttributeValue() const {
  return getAttribute(eventAttr).GetString();
}

String HTMLScriptElement::CrossOriginAttributeValue() const {
  return getAttribute(crossoriginAttr);
}

String HTMLScriptElement::IntegrityAttributeValue() const {
  return getAttribute(integrityAttr);
}

String HTMLScriptElement::TextFromChildren() {
  return Element::TextFromChildren();
}

bool HTMLScriptElement::AsyncAttributeValue() const {
  return FastHasAttribute(asyncAttr);
}

bool HTMLScriptElement::DeferAttributeValue() const {
  return FastHasAttribute(deferAttr);
}

bool HTMLScriptElement::HasSourceAttribute() const {
  return FastHasAttribute(srcAttr);
}

bool HTMLScriptElement::IsConnected() const {
  return Node::isConnected();
}

bool HTMLScriptElement::HasChildren() const {
  return Node::hasChildren();
}

const AtomicString& HTMLScriptElement::GetNonceForElement() const {
  return ContentSecurityPolicy::IsNonceableElement(this) ? nonce()
                                                         : g_null_atom;
}

bool HTMLScriptElement::AllowInlineScriptForCSP(
    const AtomicString& nonce,
    const WTF::OrdinalNumber& context_line,
    const String& script_content,
    ContentSecurityPolicy::InlineType inline_type) {
  return GetDocument().GetContentSecurityPolicy()->AllowInlineScript(
      this, GetDocument().Url(), nonce, context_line, script_content,
      inline_type);
}

Document& HTMLScriptElement::GetDocument() const {
  return Node::GetDocument();
}

void HTMLScriptElement::DispatchLoadEvent() {
  DCHECK(!loader_->HaveFiredLoadEvent());
  DispatchEvent(Event::Create(EventTypeNames::load));
}

void HTMLScriptElement::DispatchErrorEvent() {
  DispatchEvent(Event::Create(EventTypeNames::error));
}

void HTMLScriptElement::SetScriptElementForBinding(
    HTMLScriptElementOrSVGScriptElement& element) {
  if (!IsInV1ShadowTree())
    element.SetHTMLScriptElement(this);
}

Element* HTMLScriptElement::CloneElementWithoutAttributesAndChildren() {
  return new HTMLScriptElement(GetDocument(), false, loader_->AlreadyStarted(),
                               false);
}

void HTMLScriptElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(loader_);
  HTMLElement::Trace(visitor);
  ScriptElementBase::Trace(visitor);
}

void HTMLScriptElement::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(loader_);
  HTMLElement::TraceWrappers(visitor);
}

}  // namespace blink
