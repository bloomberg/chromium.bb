/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/dom/shadow/ShadowRoot.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleSheetCollections.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRootRareData.h"
#include "core/editing/markup.h"
#include "core/platform/HistogramSupport.h"

namespace WebCore {

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    void* pointers[3];
    unsigned countersAndFlags[1];
};

COMPILE_ASSERT(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot), shadowroot_should_stay_small);

enum ShadowRootUsageOriginType {
    ShadowRootUsageOriginWeb = 0,
    ShadowRootUsageOriginNotWeb,
    ShadowRootUsageOriginMax
};

ShadowRoot::ShadowRoot(Document* document, ShadowRootType type)
    : DocumentFragment(0, CreateShadowRoot)
    , TreeScope(this, document)
    , m_prev(0)
    , m_next(0)
    , m_numberOfStyles(0)
    , m_applyAuthorStyles(false)
    , m_resetStyleInheritance(false)
    , m_type(type)
    , m_registeredWithParentShadowRoot(false)
    , m_childInsertionPointsIsValid(false)
{
    ASSERT(document);
    ScriptWrappable::init(this);

    if (type == ShadowRoot::AuthorShadowRoot) {
        ShadowRootUsageOriginType usageType = document->url().protocolIsInHTTPFamily() ? ShadowRootUsageOriginWeb : ShadowRootUsageOriginNotWeb;
        HistogramSupport::histogramEnumeration("WebCore.ShadowRoot.constructor", usageType, ShadowRootUsageOriginMax);
    }
}

ShadowRoot::~ShadowRoot()
{
    ASSERT(!m_prev);
    ASSERT(!m_next);

    documentInternal()->styleSheetCollections()->didRemoveShadowRoot(this);

    // We cannot let ContainerNode destructor call willBeDeletedFrom()
    // for this ShadowRoot instance because TreeScope destructor
    // clears Node::m_treeScope thus ContainerNode is no longer able
    // to access it Document reference after that.
    willBeDeletedFrom(documentInternal());

    // We must remove all of our children first before the TreeScope destructor
    // runs so we don't go through TreeScopeAdopter for each child with a
    // destructed tree scope in each descendant.
    removeDetachedChildren();

    // We must call clearRareData() here since a ShadowRoot class inherits TreeScope
    // as well as Node. See a comment on TreeScope.h for the reason.
    if (hasRareData())
        clearRareData();
}

void ShadowRoot::dispose()
{
    removeDetachedChildren();
}

ShadowRoot* ShadowRoot::bindingsOlderShadowRoot() const
{
    ShadowRoot* older = olderShadowRoot();
    while (older && !older->shouldExposeToBindings())
        older = older->olderShadowRoot();
    ASSERT(!older || older->shouldExposeToBindings());
    return older;
}

bool ShadowRoot::isOldestAuthorShadowRoot() const
{
    if (type() != AuthorShadowRoot)
        return false;
    if (ShadowRoot* older = olderShadowRoot())
        return older->type() == UserAgentShadowRoot;
    return true;
}

PassRefPtr<Node> ShadowRoot::cloneNode(bool, ExceptionState& es)
{
    es.throwDOMException(DataCloneError, ExceptionMessages::failedToExecute("cloneNode", "ShadowRoot", "ShadowRoot nodes are not clonable."));
    return 0;
}

String ShadowRoot::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup, ExceptionState& es)
{
    if (isOrphan()) {
        es.throwDOMException(InvalidAccessError, ExceptionMessages::failedToExecute("setInnerHTML", "ShadowRoot", "The ShadowRoot does not have a host."));
        return;
    }

    if (RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, host(), AllowScriptingContent, es))
        replaceChildrenWithFragment(this, fragment.release(), es);
}

bool ShadowRoot::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        return false;
    }
}

void ShadowRoot::recalcStyle(StyleChange change)
{
    // ShadowRoot doesn't support custom callbacks.
    ASSERT(!hasCustomStyleCallbacks());

    StyleResolver* styleResolver = document().styleResolver();
    styleResolver->pushParentShadowRoot(*this);

    if (!attached()) {
        attach();
        return;
    }

    // When we're set to lazyAttach we'll have a SubtreeStyleChange and we'll need
    // to promote the change to a Force for all our descendants so they get a
    // recalc and will attach.
    if (styleChangeType() >= SubtreeStyleChange)
        change = Force;

    // FIXME: This doesn't handle :hover + div properly like Element::recalcStyle does.
    bool forceReattachOfAnyWhitespaceSibling = false;
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        bool didReattach = false;

        if (child->renderer())
            forceReattachOfAnyWhitespaceSibling = false;

        if (child->isTextNode()) {
            if (forceReattachOfAnyWhitespaceSibling && toText(child)->containsOnlyWhitespace())
                child->reattach();
            else
                didReattach = toText(child)->recalcTextStyle(change);
        } else if (child->isElementNode() && shouldRecalcStyle(change, child)) {
            didReattach = toElement(child)->recalcStyle(change);
        }

        forceReattachOfAnyWhitespaceSibling = didReattach || forceReattachOfAnyWhitespaceSibling;
    }

    styleResolver->popParentShadowRoot(*this);
    clearNeedsStyleRecalc();
    clearChildNeedsStyleRecalc();
}

bool ShadowRoot::isActive() const
{
    for (ShadowRoot* shadowRoot = youngerShadowRoot(); shadowRoot; shadowRoot = shadowRoot->youngerShadowRoot())
        if (!shadowRoot->containsShadowElements())
            return false;
    return true;
}

void ShadowRoot::setApplyAuthorStyles(bool value)
{
    if (isOrphan())
        return;

    if (m_applyAuthorStyles == value)
        return;

    m_applyAuthorStyles = value;
    if (!isActive())
        return;

    ASSERT(host());
    ASSERT(host()->shadow());
    if (host()->shadow()->didAffectApplyAuthorStyles())
        host()->setNeedsStyleRecalc();

    // Since styles in shadow trees can select shadow hosts, set shadow host's needs-recalc flag true.
    // FIXME: host->setNeedsStyleRecalc() should take care of all elements in its shadow tree.
    // However, when host's recalcStyle is skipped (i.e. host's parent has no renderer),
    // no recalc style is invoked for any elements in its shadow tree.
    // This problem occurs when using getComputedStyle() API.
    // So currently host and shadow root's needsStyleRecalc flags are set to be true.
    setNeedsStyleRecalc();
}

void ShadowRoot::setResetStyleInheritance(bool value)
{
    if (isOrphan())
        return;

    if (value == m_resetStyleInheritance)
        return;

    m_resetStyleInheritance = value;
    if (!isActive())
        return;

    setNeedsStyleRecalc();
}

void ShadowRoot::attach(const AttachContext& context)
{
    StyleResolver* styleResolver = document().styleResolver();
    styleResolver->pushParentShadowRoot(*this);
    DocumentFragment::attach(context);
    styleResolver->popParentShadowRoot(*this);
}

Node::InsertionNotificationRequest ShadowRoot::insertedInto(ContainerNode* insertionPoint)
{
    DocumentFragment::insertedInto(insertionPoint);

    if (!insertionPoint->inDocument() || !isOldest())
        return InsertionDone;

    // FIXME: When parsing <video controls>, insertedInto() is called many times without invoking removedFrom.
    // For now, we check m_registeredWithParentShadowroot. We would like to ASSERT(!m_registeredShadowRoot) here.
    // https://bugs.webkit.org/show_bug.cig?id=101316
    if (m_registeredWithParentShadowRoot)
        return InsertionDone;

    if (ShadowRoot* root = host()->containingShadowRoot()) {
        root->addChildShadowRoot();
        m_registeredWithParentShadowRoot = true;
    }

    return InsertionDone;
}

void ShadowRoot::removedFrom(ContainerNode* insertionPoint)
{
    if (insertionPoint->inDocument() && m_registeredWithParentShadowRoot) {
        ShadowRoot* root = host()->containingShadowRoot();
        if (!root)
            root = insertionPoint->containingShadowRoot();
        if (root)
            root->removeChildShadowRoot();
        m_registeredWithParentShadowRoot = false;
    }

    DocumentFragment::removedFrom(insertionPoint);
}

void ShadowRoot::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (InsertionPoint* point = insertionPoint()) {
        if (ShadowRoot* root = point->containingShadowRoot())
            root->owner()->setNeedsDistributionRecalc();
    }
}

void ShadowRoot::registerScopedHTMLStyleChild()
{
    ++m_numberOfStyles;
    setHasScopedHTMLStyleChild(true);
}

void ShadowRoot::unregisterScopedHTMLStyleChild()
{
    ASSERT(hasScopedHTMLStyleChild() && m_numberOfStyles > 0);
    --m_numberOfStyles;
    setHasScopedHTMLStyleChild(m_numberOfStyles > 0);
}

ShadowRootRareData* ShadowRoot::ensureShadowRootRareData()
{
    if (m_shadowRootRareData)
        return m_shadowRootRareData.get();

    m_shadowRootRareData = adoptPtr(new ShadowRootRareData);
    return m_shadowRootRareData.get();
}

bool ShadowRoot::containsShadowElements() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->hasShadowElementChildren() : 0;
}

bool ShadowRoot::containsContentElements() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->hasContentElementChildren() : 0;
}

bool ShadowRoot::containsShadowRoots() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->hasShadowRootChildren() : 0;
}

InsertionPoint* ShadowRoot::insertionPoint() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->insertionPoint() : 0;
}

void ShadowRoot::setInsertionPoint(PassRefPtr<InsertionPoint> insertionPoint)
{
    if (!m_shadowRootRareData && !insertionPoint)
        return;
    ensureShadowRootRareData()->setInsertionPoint(insertionPoint);
}

void ShadowRoot::addInsertionPoint(InsertionPoint* insertionPoint)
{
    ensureShadowRootRareData()->addInsertionPoint(insertionPoint);
    invalidateChildInsertionPoints();
}

void ShadowRoot::removeInsertionPoint(InsertionPoint* insertionPoint)
{
    m_shadowRootRareData->removeInsertionPoint(insertionPoint);
    invalidateChildInsertionPoints();
}

void ShadowRoot::addChildShadowRoot()
{
    ensureShadowRootRareData()->addChildShadowRoot();
}

void ShadowRoot::removeChildShadowRoot()
{
    // FIXME: Why isn't this an ASSERT?
    if (!m_shadowRootRareData)
        return;
    m_shadowRootRareData->removeChildShadowRoot();
}

unsigned ShadowRoot::childShadowRootCount() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->childShadowRootCount() : 0;
}

void ShadowRoot::invalidateChildInsertionPoints()
{
    m_childInsertionPointsIsValid = false;
    m_shadowRootRareData->clearChildInsertionPoints();
}

const Vector<RefPtr<InsertionPoint> >& ShadowRoot::childInsertionPoints()
{
    DEFINE_STATIC_LOCAL(const Vector<RefPtr<InsertionPoint> >, emptyList, ());

    if (m_shadowRootRareData && m_childInsertionPointsIsValid)
        return m_shadowRootRareData->childInsertionPoints();

    m_childInsertionPointsIsValid = true;

    if (!containsInsertionPoints())
        return emptyList;

    Vector<RefPtr<InsertionPoint> > insertionPoints;
    for (Element* element = ElementTraversal::firstWithin(this); element; element = ElementTraversal::next(element, this)) {
        if (element->isInsertionPoint())
            insertionPoints.append(toInsertionPoint(element));
    }

    ensureShadowRootRareData()->setChildInsertionPoints(insertionPoints);

    return m_shadowRootRareData->childInsertionPoints();
}

}
