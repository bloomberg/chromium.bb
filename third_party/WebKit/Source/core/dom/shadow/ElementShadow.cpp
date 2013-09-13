/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/dom/shadow/ElementShadow.h"

#include "core/dom/ContainerNodeAlgorithms.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ContentDistribution.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/shadow/HTMLContentElement.h"
#include "core/html/shadow/HTMLShadowElement.h"

namespace WebCore {

PassOwnPtr<ElementShadow> ElementShadow::create()
{
    return adoptPtr(new ElementShadow());
}

ElementShadow::ElementShadow()
    : m_needsDistributionRecalc(false)
    , m_applyAuthorStyles(false)
    , m_needsSelectFeatureSet(false)
{
}

ElementShadow::~ElementShadow()
{
    removeAllShadowRoots();
}

ShadowRoot* ElementShadow::addShadowRoot(Element* shadowHost, ShadowRoot::ShadowRootType type)
{
    RefPtr<ShadowRoot> shadowRoot = ShadowRoot::create(&shadowHost->document(), type);

    shadowRoot->setParentOrShadowHostNode(shadowHost);
    shadowRoot->setParentTreeScope(&shadowHost->treeScope());
    m_shadowRoots.push(shadowRoot.get());
    ChildNodeInsertionNotifier(shadowHost).notify(shadowRoot.get());
    setNeedsDistributionRecalc();
    shadowHost->lazyReattachIfAttached();

    // addShadowRoot() affects apply-author-styles. However, we know that the youngest shadow root has not had any children yet.
    // The youngest shadow root's apply-author-styles is default (false). So we can just set m_applyAuthorStyles false.
    m_applyAuthorStyles = false;

    shadowHost->didAddShadowRoot(*shadowRoot);
    InspectorInstrumentation::didPushShadowRoot(shadowHost, shadowRoot.get());

    return shadowRoot.get();
}

void ElementShadow::removeAllShadowRoots()
{
    // Dont protect this ref count.
    Element* shadowHost = host();

    while (RefPtr<ShadowRoot> oldRoot = m_shadowRoots.head()) {
        InspectorInstrumentation::willPopShadowRoot(shadowHost, oldRoot.get());
        shadowHost->document().removeFocusedElementOfSubtree(oldRoot.get());

        if (oldRoot->attached())
            oldRoot->detach();

        m_shadowRoots.removeHead();
        oldRoot->setParentOrShadowHostNode(0);
        oldRoot->setParentTreeScope(&shadowHost->document());
        oldRoot->setPrev(0);
        oldRoot->setNext(0);
        ChildNodeRemovalNotifier(shadowHost).notify(oldRoot.get());
    }
}

void ElementShadow::attach(const Node::AttachContext& context)
{
    Node::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!root->attached())
            root->attach(childrenContext);
    }
}

void ElementShadow::detach(const Node::AttachContext& context)
{
    Node::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->attached())
            root->detach(childrenContext);
    }
}

void ElementShadow::removeAllEventListeners()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        for (Node* node = root; node; node = NodeTraversal::next(node))
            node->removeAllEventListeners();
    }
}

void ElementShadow::setNeedsDistributionRecalc()
{
    if (m_needsDistributionRecalc)
        return;
    m_needsDistributionRecalc = true;
    host()->markAncestorsWithChildNeedsDistributionRecalc();
    clearDistribution();
}

bool ElementShadow::didAffectApplyAuthorStyles()
{
    bool applyAuthorStyles = resolveApplyAuthorStyles();

    if (m_applyAuthorStyles == applyAuthorStyles)
        return false;

    m_applyAuthorStyles = applyAuthorStyles;
    return true;
}

bool ElementShadow::containsActiveStyles() const
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->hasScopedHTMLStyleChild())
            return true;
        if (!root->containsShadowElements())
            return false;
    }
    return false;
}

bool ElementShadow::resolveApplyAuthorStyles() const
{
    for (const ShadowRoot* shadowRoot = youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
        if (shadowRoot->applyAuthorStyles())
            return true;
        if (!shadowRoot->containsShadowElements())
            break;
    }
    return false;
}

InsertionPoint* ElementShadow::findInsertionPointFor(const Node* key) const
{
    return m_nodeToInsertionPoint.get(key);
}

void ElementShadow::populate(Node* node, Vector<Node*>& pool)
{
    if (!isActiveInsertionPoint(node)) {
        pool.append(node);
        return;
    }

    InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (insertionPoint->hasDistribution()) {
        for (size_t i = 0; i < insertionPoint->size(); ++i)
            pool.append(insertionPoint->at(i));
    } else {
        for (Node* fallbackNode = insertionPoint->firstChild(); fallbackNode; fallbackNode = fallbackNode->nextSibling())
            pool.append(fallbackNode);
    }
}

void ElementShadow::distribute()
{
    Vector<Node*> pool;
    pool.reserveInitialCapacity(32);
    for (Node* node = host()->firstChild(); node; node = node->nextSibling())
        populate(node, pool);

    host()->setNeedsStyleRecalc();

    Vector<bool> distributed;
    distributed.fill(false, pool.size());

    Vector<HTMLShadowElement*, 32> activeShadowInsertionPoints;
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        HTMLShadowElement* firstActiveShadowInsertionPoint = 0;

        const Vector<RefPtr<InsertionPoint> >& insertionPoints = root->childInsertionPoints();
        for (size_t i = 0; i < insertionPoints.size(); ++i) {
            InsertionPoint* point = insertionPoints[i].get();
            if (!point->isActive())
                continue;

            if (isHTMLShadowElement(point)) {
                if (!firstActiveShadowInsertionPoint)
                    firstActiveShadowInsertionPoint = toHTMLShadowElement(point);
            } else {
                distributeSelectionsTo(point, pool, distributed);
                if (ElementShadow* shadow = shadowOfParentForDistribution(point))
                    shadow->setNeedsDistributionRecalc();
            }
        }

        if (firstActiveShadowInsertionPoint)
            activeShadowInsertionPoints.append(firstActiveShadowInsertionPoint);
    }

    for (size_t i = activeShadowInsertionPoints.size(); i > 0; --i) {
        HTMLShadowElement* shadowElement = activeShadowInsertionPoints[i - 1];
        ShadowRoot* root = shadowElement->containingShadowRoot();
        ASSERT(root);
        if (root->olderShadowRoot() && root->olderShadowRoot()->type() == root->type()) {
            // Only allow reprojecting older shadow roots between the same type to
            // disallow reprojecting UA elements into author shadows.
            distributeNodeChildrenTo(shadowElement, root->olderShadowRoot());
            root->olderShadowRoot()->setInsertionPoint(shadowElement);
        } else if (!root->olderShadowRoot()) {
            // There's assumed to always be a UA shadow that selects all nodes.
            // We don't actually add it, instead we just distribute the pool to the
            // <shadow> in the oldest shadow root.
            distributeSelectionsTo(shadowElement, pool, distributed);
        }
        if (ElementShadow* shadow = shadowOfParentForDistribution(shadowElement))
            shadow->setNeedsDistributionRecalc();
    }

    // Detach all nodes that were not distributed and have a renderer.
    for (size_t i = 0; i < pool.size(); ++i) {
        if (distributed[i])
            continue;
        if (pool[i]->renderer())
            pool[i]->lazyReattachIfAttached();
    }
}

void ElementShadow::distributeSelectionsTo(InsertionPoint* insertionPoint, const Vector<Node*>& pool, Vector<bool>& distributed)
{
    ContentDistribution distribution;

    for (size_t i = 0; i < pool.size(); ++i) {
        if (distributed[i])
            continue;

        if (isHTMLContentElement(insertionPoint) && !toHTMLContentElement(insertionPoint)->canSelectNode(pool, i))
            continue;

        Node* child = pool[i];
        distribution.append(child);
        m_nodeToInsertionPoint.add(child, insertionPoint);
        distributed[i] = true;
    }

    insertionPoint->setDistribution(distribution);
}

void ElementShadow::distributeNodeChildrenTo(InsertionPoint* insertionPoint, ContainerNode* containerNode)
{
    ContentDistribution distribution;
    for (Node* node = containerNode->firstChild(); node; node = node->nextSibling()) {
        if (isActiveInsertionPoint(node)) {
            InsertionPoint* innerInsertionPoint = toInsertionPoint(node);
            if (innerInsertionPoint->hasDistribution()) {
                for (size_t i = 0; i < innerInsertionPoint->size(); ++i) {
                    Node* nodeToAdd = innerInsertionPoint->at(i);
                    distribution.append(nodeToAdd);
                    m_nodeToInsertionPoint.add(nodeToAdd, insertionPoint);
                }
            } else {
                for (Node* child = innerInsertionPoint->firstChild(); child; child = child->nextSibling()) {
                    distribution.append(child);
                    m_nodeToInsertionPoint.add(child, insertionPoint);
                }
            }
        } else {
            distribution.append(node);
            m_nodeToInsertionPoint.add(node, insertionPoint);
        }
    }

    insertionPoint->setDistribution(distribution);
}

const SelectRuleFeatureSet& ElementShadow::ensureSelectFeatureSet()
{
    if (!m_needsSelectFeatureSet)
        return m_selectFeatures;

    m_selectFeatures.clear();
    for (ShadowRoot* root = oldestShadowRoot(); root; root = root->youngerShadowRoot())
        collectSelectFeatureSetFrom(root);
    m_needsSelectFeatureSet = false;
    return m_selectFeatures;
}

void ElementShadow::collectSelectFeatureSetFrom(ShadowRoot* root)
{
    if (!root->containsShadowRoots() && !root->containsContentElements())
        return;

    for (Element* element = ElementTraversal::firstWithin(root); element; element = ElementTraversal::next(element, root)) {
        if (ElementShadow* shadow = element->shadow())
            m_selectFeatures.add(shadow->ensureSelectFeatureSet());
        if (!isHTMLContentElement(element))
            continue;
        const CSSSelectorList& list = toHTMLContentElement(element)->selectorList();
        for (const CSSSelector* selector = list.first(); selector; selector = CSSSelectorList::next(selector)) {
            for (const CSSSelector* component = selector; component; component = component->tagHistory())
                m_selectFeatures.collectFeaturesFromSelector(component);
        }
    }
}

void ElementShadow::didAffectSelector(AffectedSelectorMask mask)
{
    if (ensureSelectFeatureSet().hasSelectorFor(mask))
        setNeedsDistributionRecalc();
}

void ElementShadow::willAffectSelector()
{
    for (ElementShadow* shadow = this; shadow; shadow = shadow->containingShadow()) {
        if (shadow->needsSelectFeatureSet())
            break;
        shadow->setNeedsSelectFeatureSet();
    }
    setNeedsDistributionRecalc();
}

void ElementShadow::clearDistribution()
{
    m_nodeToInsertionPoint.clear();

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->setInsertionPoint(0);
}

} // namespace
