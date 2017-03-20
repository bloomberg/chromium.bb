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
                                          bool wasInsertedByParser,
                                          bool alreadyStarted)
    : SVGElement(SVGNames::scriptTag, document), SVGURIReference(this) {
  initializeScriptLoader(wasInsertedByParser, alreadyStarted, false);
}

SVGScriptElement* SVGScriptElement::create(Document& document,
                                           bool insertedByParser) {
  return new SVGScriptElement(document, insertedByParser, false);
}

void SVGScriptElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == HTMLNames::onerrorAttr) {
    setAttributeEventListener(
        EventTypeNames::error,
        createAttributeEventListener(this, params.name, params.newValue,
                                     eventParameterName()));
  } else if (params.name == HTMLNames::nonceAttr) {
    if (params.newValue == ContentSecurityPolicy::getNonceReplacementString())
      return;
    setNonce(params.newValue);
    if (RuntimeEnabledFeatures::hideNonceContentAttributeEnabled()) {
      setAttribute(HTMLNames::nonceAttr,
                   ContentSecurityPolicy::getNonceReplacementString());
    }
  } else {
    SVGElement::parseAttribute(params);
  }
}

void SVGScriptElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (SVGURIReference::isKnownAttribute(attrName)) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    m_loader->handleSourceAttribute(hrefString());
    return;
  }

  SVGElement::svgAttributeChanged(attrName);
}

Node::InsertionNotificationRequest SVGScriptElement::insertedInto(
    ContainerNode* rootParent) {
  SVGElement::insertedInto(rootParent);
  return InsertionShouldCallDidNotifySubtreeInsertions;
}

void SVGScriptElement::didNotifySubtreeInsertionsToDocument() {
  m_loader->didNotifySubtreeInsertionsToDocument();

  if (!m_loader->isParserInserted())
    m_loader->setHaveFiredLoadEvent(true);
}

void SVGScriptElement::childrenChanged(const ChildrenChange& change) {
  SVGElement::childrenChanged(change);
  m_loader->childrenChanged();
}

void SVGScriptElement::didMoveToNewDocument(Document& oldDocument) {
  ScriptRunner::movePendingScript(oldDocument, document(), m_loader.get());
  SVGElement::didMoveToNewDocument(oldDocument);
}

bool SVGScriptElement::isURLAttribute(const Attribute& attribute) const {
  return attribute.name() == AtomicString(sourceAttributeValue());
}

void SVGScriptElement::finishParsingChildren() {
  SVGElement::finishParsingChildren();
  m_loader->setHaveFiredLoadEvent(true);
}

bool SVGScriptElement::haveLoadedRequiredResources() {
  return m_loader->haveFiredLoadEvent();
}

String SVGScriptElement::sourceAttributeValue() const {
  return hrefString();
}

String SVGScriptElement::typeAttributeValue() const {
  return getAttribute(SVGNames::typeAttr).getString();
}

String SVGScriptElement::textFromChildren() {
  return Element::textFromChildren();
}

String SVGScriptElement::textContent() const {
  return Node::textContent();
}

bool SVGScriptElement::hasSourceAttribute() const {
  return href()->isSpecified();
}

bool SVGScriptElement::isConnected() const {
  return Node::isConnected();
}

bool SVGScriptElement::hasChildren() const {
  return Node::hasChildren();
}

bool SVGScriptElement::isNonceableElement() const {
  return ContentSecurityPolicy::isNonceableElement(this);
}

bool SVGScriptElement::allowInlineScriptForCSP(
    const AtomicString& nonce,
    const WTF::OrdinalNumber& contextLine,
    const String& scriptContent) {
  return document().contentSecurityPolicy()->allowInlineScript(
      this, document().url(), nonce, contextLine, scriptContent);
}

AtomicString SVGScriptElement::initiatorName() const {
  return Element::localName();
}

Document& SVGScriptElement::document() const {
  return Node::document();
}

Element* SVGScriptElement::cloneElementWithoutAttributesAndChildren() {
  return new SVGScriptElement(document(), false, m_loader->alreadyStarted());
}

void SVGScriptElement::dispatchLoadEvent() {
  dispatchEvent(Event::create(EventTypeNames::load));
}

void SVGScriptElement::dispatchErrorEvent() {
  dispatchEvent(Event::create(EventTypeNames::error));
}

void SVGScriptElement::setScriptElementForBinding(
    HTMLScriptElementOrSVGScriptElement& element) {
  if (!isInV1ShadowTree())
    element.setSVGScriptElement(this);
}

#if DCHECK_IS_ON()
bool SVGScriptElement::isAnimatableAttribute(const QualifiedName& name) const {
  if (name == SVGNames::typeAttr || name == SVGNames::hrefAttr ||
      name == XLinkNames::hrefAttr)
    return false;
  return SVGElement::isAnimatableAttribute(name);
}
#endif

DEFINE_TRACE(SVGScriptElement) {
  SVGElement::trace(visitor);
  SVGURIReference::trace(visitor);
  ScriptElementBase::trace(visitor);
}

}  // namespace blink
