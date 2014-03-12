/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
 *           (C) 2007 Rob Buis (buis@kde.org)
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

#include "config.h"
#include "core/html/HTMLStyleElement.h"

#include "HTMLNames.h"
#include "core/css/MediaList.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/events/EventSender.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

using namespace HTMLNames;

static StyleEventSender& styleLoadEventSender()
{
    DEFINE_STATIC_LOCAL(StyleEventSender, sharedLoadEventSender, (EventTypeNames::load));
    return sharedLoadEventSender;
}

inline HTMLStyleElement::HTMLStyleElement(Document& document, bool createdByParser)
    : HTMLElement(styleTag, document)
    , StyleElement(&document, createdByParser)
    , m_firedLoad(false)
    , m_loadedSheet(false)
    , m_scopedStyleRegistrationState(NotRegistered)
{
    ScriptWrappable::init(this);
}

HTMLStyleElement::~HTMLStyleElement()
{
    // During tear-down, willRemove isn't called, so m_scopedStyleRegistrationState may still be RegisteredAsScoped or RegisteredInShadowRoot here.
    // Therefore we can't ASSERT(m_scopedStyleRegistrationState == NotRegistered).
    StyleElement::clearDocumentData(document(), this);

    styleLoadEventSender().cancelEvent(this);
}

PassRefPtr<HTMLStyleElement> HTMLStyleElement::create(Document& document, bool createdByParser)
{
    return adoptRef(new HTMLStyleElement(document, createdByParser));
}

void HTMLStyleElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == titleAttr && m_sheet) {
        m_sheet->setTitle(value);
    } else if (name == scopedAttr && ContextFeatures::styleScopedEnabled(&document())) {
        scopedAttributeChanged(!value.isNull());
    } else if (name == mediaAttr && inDocument() && document().isActive() && m_sheet) {
        m_sheet->setMediaQueries(MediaQuerySet::create(value));
        // FIXME: This shold be RecalcStyleDeferred.
        document().modifiedStyleSheet(m_sheet.get(), RecalcStyleImmediately);
    } else {
        HTMLElement::parseAttribute(name, value);
    }
}

void HTMLStyleElement::scopedAttributeChanged(bool scoped)
{
    ASSERT(ContextFeatures::styleScopedEnabled(&document()));

    if (!inDocument())
        return;

    if (scoped) {
        if (m_scopedStyleRegistrationState == RegisteredAsScoped)
            return;

        // As any <style> in a shadow tree is treated as "scoped",
        // need to remove the <style> from its shadow root.
        ContainerNode* scopingNode = 0;
        if (m_scopedStyleRegistrationState == RegisteredInShadowRoot) {
            scopingNode = containingShadowRoot();
            unregisterWithScopingNode(scopingNode);
        }
        document().styleEngine()->removeStyleSheetCandidateNode(this, scopingNode, treeScope());
        registerWithScopingNode(true);

        document().styleEngine()->addStyleSheetCandidateNode(this, false);
        document().modifiedStyleSheet(sheet());
        return;
    }

    // If the <style> was scoped, need to remove the <style> from the scoping
    // element, i.e. the parent node.
    if (m_scopedStyleRegistrationState != RegisteredAsScoped)
        return;

    unregisterWithScopingNode(parentNode());
    document().styleEngine()->removeStyleSheetCandidateNode(this, parentNode(), treeScope());

    // As any <style> in a shadow tree is treated as "scoped",
    // need to add the <style> to its shadow root.
    if (isInShadowTree())
        registerWithScopingNode(false);

    document().styleEngine()->addStyleSheetCandidateNode(this, false);
    // FIXME: currently need to use FullStyleUpdate here.
    // Because ShadowTreeStyleSheetCollection doesn't know old scoping node.
    // So setNeedsStyleRecalc for old scoping node is not invoked.
    document().modifiedStyleSheet(sheet());
}

void HTMLStyleElement::finishParsingChildren()
{
    StyleElement::finishParsingChildren(this);
    HTMLElement::finishParsingChildren();
}

void HTMLStyleElement::registerWithScopingNode(bool scoped)
{
    // Note: We cannot rely on the 'scoped' element already being present when this method is invoked.
    // Therefore we cannot rely on scoped()!
    ASSERT(m_scopedStyleRegistrationState == NotRegistered);
    ASSERT(inDocument());
    if (m_scopedStyleRegistrationState != NotRegistered)
        return;

    ContainerNode* scope = scoped ? parentNode() : containingShadowRoot();
    if (!scope)
        return;
    if (!scope->isElementNode() && !scope->isShadowRoot()) {
        // DocumentFragment nodes should never be inDocument,
        // <style> should not be a child of Document, PI or some such.
        ASSERT_NOT_REACHED();
        return;
    }
    scope->registerScopedHTMLStyleChild();
    m_scopedStyleRegistrationState = scoped ? RegisteredAsScoped : RegisteredInShadowRoot;
}

void HTMLStyleElement::unregisterWithScopingNode(ContainerNode* scope)
{
    ASSERT(m_scopedStyleRegistrationState != NotRegistered || !ContextFeatures::styleScopedEnabled(&document()));
    if (!isRegisteredAsScoped())
        return;

    ASSERT(scope);
    if (scope) {
        ASSERT(scope->hasScopedHTMLStyleChild());
        scope->unregisterScopedHTMLStyleChild();
    }

    m_scopedStyleRegistrationState = NotRegistered;
}

Node::InsertionNotificationRequest HTMLStyleElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument()) {
        if (m_scopedStyleRegistrationState == NotRegistered && (scoped() || isInShadowTree()))
            registerWithScopingNode(scoped());
    }
    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLStyleElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);

    // In the current implementation, <style scoped> is only registered if the node is in the document.
    // That is, because willRemove() is also called if an ancestor is removed from the document.
    // Now, if we want to register <style scoped> even if it's not inDocument,
    // we'd need to find a way to discern whether that is the case, or whether <style scoped> itself is about to be removed.
    ContainerNode* scopingNode = 0;
    if (m_scopedStyleRegistrationState != NotRegistered) {
        if (m_scopedStyleRegistrationState == RegisteredInShadowRoot) {
            scopingNode = containingShadowRoot();
            if (!scopingNode)
                scopingNode = insertionPoint->containingShadowRoot();
        } else {
            scopingNode = parentNode() ? parentNode() : insertionPoint;
        }

        unregisterWithScopingNode(scopingNode);
    }

    if (insertionPoint->inDocument()) {
        TreeScope* containingScope = containingShadowRoot();
        StyleElement::removedFromDocument(document(), this, scopingNode, containingScope ? *containingScope : insertionPoint->treeScope());
    }
}

void HTMLStyleElement::didNotifySubtreeInsertionsToDocument()
{
    StyleElement::processStyleSheet(document(), this);
}

void HTMLStyleElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    StyleElement::childrenChanged(this);
}

const AtomicString& HTMLStyleElement::media() const
{
    return getAttribute(mediaAttr);
}

const AtomicString& HTMLStyleElement::type() const
{
    return getAttribute(typeAttr);
}

bool HTMLStyleElement::scoped() const
{
    return fastHasAttribute(scopedAttr) && ContextFeatures::styleScopedEnabled(&document());
}

void HTMLStyleElement::setScoped(bool scopedValue)
{
    setBooleanAttribute(scopedAttr, scopedValue);
}

ContainerNode* HTMLStyleElement::scopingNode()
{
    if (!inDocument())
        return 0;

    if (!isRegisteredAsScoped())
        return &document();

    if (isRegisteredInShadowRoot())
        return containingShadowRoot();

    return parentNode();
}

void HTMLStyleElement::dispatchPendingLoadEvents()
{
    styleLoadEventSender().dispatchPendingEvents();
}

void HTMLStyleElement::dispatchPendingEvent(StyleEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &styleLoadEventSender());
    dispatchEvent(Event::create(m_loadedSheet ? EventTypeNames::load : EventTypeNames::error));
}

void HTMLStyleElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    if (m_firedLoad)
        return;
    m_loadedSheet = !errorOccurred;
    styleLoadEventSender().dispatchEventSoon(this);
    m_firedLoad = true;
}

bool HTMLStyleElement::disabled() const
{
    if (!m_sheet)
        return false;

    return m_sheet->disabled();
}

void HTMLStyleElement::setDisabled(bool setDisabled)
{
    if (CSSStyleSheet* styleSheet = sheet())
        styleSheet->setDisabled(setDisabled);
}

}
