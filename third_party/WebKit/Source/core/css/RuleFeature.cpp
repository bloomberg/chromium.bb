/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/css/RuleFeature.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/RuleSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/rendering/RenderObject.h"
#include "wtf/BitVector.h"

namespace WebCore {

static bool isSkippableComponentForInvalidation(const CSSSelector& selector)
{
    if (selector.m_match == CSSSelector::Tag
        || selector.m_match == CSSSelector::Id
        || selector.isAttributeSelector())
        return true;
    if (selector.m_match != CSSSelector::PseudoClass)
        return false;
    switch (selector.pseudoType()) {
    case CSSSelector::PseudoEmpty:
    case CSSSelector::PseudoFirstChild:
    case CSSSelector::PseudoFirstOfType:
    case CSSSelector::PseudoLastChild:
    case CSSSelector::PseudoLastOfType:
    case CSSSelector::PseudoOnlyChild:
    case CSSSelector::PseudoOnlyOfType:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthLastOfType:
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoAnyLink:
    case CSSSelector::PseudoHover:
    case CSSSelector::PseudoDrag:
    case CSSSelector::PseudoFocus:
    case CSSSelector::PseudoActive:
    case CSSSelector::PseudoChecked:
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoDefault:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoOptional:
    case CSSSelector::PseudoRequired:
    case CSSSelector::PseudoReadOnly:
    case CSSSelector::PseudoReadWrite:
    case CSSSelector::PseudoValid:
    case CSSSelector::PseudoInvalid:
    case CSSSelector::PseudoIndeterminate:
    case CSSSelector::PseudoTarget:
    case CSSSelector::PseudoLang:
    case CSSSelector::PseudoRoot:
    case CSSSelector::PseudoScope:
    case CSSSelector::PseudoInRange:
    case CSSSelector::PseudoOutOfRange:
    case CSSSelector::PseudoUnresolved:
        return true;
    default:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// This method is somewhat conservative in what it accepts.
static bool supportsClassDescendantInvalidation(const CSSSelector& selector)
{
    bool foundDescendantRelation = false;
    bool foundIdent = false;
    for (const CSSSelector* component = &selector; component; component = component->tagHistory()) {

        // FIXME: We should allow pseudo elements, but we need to change how they hook
        // into recalcStyle by moving them to recalcOwnStyle instead of recalcChildStyle.

        // FIXME: next up: Tag and Id.
        if (component->m_match == CSSSelector::Class) {
            if (!foundDescendantRelation)
                foundIdent = true;
        } else if (!isSkippableComponentForInvalidation(*component)) {
            return false;
        }
        // FIXME: We can probably support ChildTree and DescendantTree.
        switch (component->relation()) {
        case CSSSelector::Descendant:
        case CSSSelector::Child:
            foundDescendantRelation = true;
            // Fall through!
        case CSSSelector::SubSelector:
            continue;
        default:
            return false;
        }
    }
    return foundIdent;
}

void extractClassIdOrTag(const CSSSelector& selector, Vector<AtomicString>& classes, AtomicString& id, AtomicString& tagName)
{
    if (selector.m_match == CSSSelector::Tag)
        tagName = selector.tagQName().localName();
    else if (selector.m_match == CSSSelector::Id)
        id = selector.value();
    else if (selector.m_match == CSSSelector::Class)
        classes.append(selector.value());
}

RuleFeatureSet::RuleFeatureSet()
    : m_targetedStyleRecalcEnabled(RuntimeEnabledFeatures::targetedStyleRecalcEnabled())
{
}

bool RuleFeatureSet::updateClassInvalidationSets(const CSSSelector& selector)
{
    if (!supportsClassDescendantInvalidation(selector))
        return false;

    Vector<AtomicString> classes;
    AtomicString id;
    AtomicString tagName;

    const CSSSelector* lastSelector = &selector;
    for (; lastSelector->relation() == CSSSelector::SubSelector; lastSelector = lastSelector->tagHistory()) {
        extractClassIdOrTag(*lastSelector, classes, id, tagName);
    }
    extractClassIdOrTag(*lastSelector, classes, id, tagName);

    for (const CSSSelector* current = &selector ; current; current = current->tagHistory()) {
        if (current->m_match == CSSSelector::Class) {
            DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(current->value());
            if (!id.isEmpty())
                invalidationSet.addId(id);
            if (!tagName.isEmpty())
                invalidationSet.addTagName(tagName);
            for (Vector<AtomicString>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
                invalidationSet.addClass(*it);
            }
        }
    }
    return true;
}

void RuleFeatureSet::addAttributeInASelector(const AtomicString& attributeName)
{
    m_metadata.attrsInRules.add(attributeName);
}

void RuleFeatureSet::collectFeaturesFromRuleData(const RuleData& ruleData)
{
    FeatureMetadata metadata;
    bool selectorUsesClassInvalidationSet = false;
    if (m_targetedStyleRecalcEnabled)
        selectorUsesClassInvalidationSet = updateClassInvalidationSets(ruleData.selector());

    SelectorFeatureCollectionMode collectionMode;
    if (selectorUsesClassInvalidationSet)
        collectionMode = DontProcessClasses;
    else
        collectionMode = ProcessClasses;
    collectFeaturesFromSelector(ruleData.selector(), metadata, collectionMode);
    m_metadata.add(metadata);

    if (metadata.foundSiblingSelector)
        siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
}

bool RuleFeatureSet::classInvalidationRequiresSubtreeRecalc(const AtomicString& className)
{
    DescendantInvalidationSet* set = m_classInvalidationSets.get(className);
    return set && set->wholeSubtreeInvalid();
}

DescendantInvalidationSet& RuleFeatureSet::ensureClassInvalidationSet(const AtomicString& className)
{
    InvalidationSetMap::AddResult addResult = m_classInvalidationSets.add(className, 0);
    if (addResult.isNewEntry)
        addResult.storedValue->value = DescendantInvalidationSet::create();
    return *addResult.storedValue->value;
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector)
{
    collectFeaturesFromSelector(selector, m_metadata, ProcessClasses);
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector, RuleFeatureSet::FeatureMetadata& metadata, SelectorFeatureCollectionMode collectionMode)
{
    unsigned maxDirectAdjacentSelectors = 0;

    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        if (current->m_match == CSSSelector::Id) {
            metadata.idsInRules.add(current->value());
        } else if (current->m_match == CSSSelector::Class && collectionMode == ProcessClasses) {
            DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(current->value());
            invalidationSet.setWholeSubtreeInvalid();
        } else if (current->isAttributeSelector()) {
            metadata.attrsInRules.add(current->attribute().localName());
        }
        if (current->pseudoType() == CSSSelector::PseudoFirstLine)
            metadata.usesFirstLineRules = true;
        if (current->isDirectAdjacentSelector()) {
            maxDirectAdjacentSelectors++;
        } else if (maxDirectAdjacentSelectors) {
            if (maxDirectAdjacentSelectors > metadata.maxDirectAdjacentSelectors)
                metadata.maxDirectAdjacentSelectors = maxDirectAdjacentSelectors;
            maxDirectAdjacentSelectors = 0;
        }
        if (current->isSiblingSelector())
            metadata.foundSiblingSelector = true;

        collectFeaturesFromSelectorList(current->selectorList(), metadata, collectionMode);
    }

    ASSERT(!maxDirectAdjacentSelectors);
}

void RuleFeatureSet::collectFeaturesFromSelectorList(const CSSSelectorList* selectorList, RuleFeatureSet::FeatureMetadata& metadata, SelectorFeatureCollectionMode collectionMode)
{
    if (!selectorList)
        return;

    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector))
        collectFeaturesFromSelector(*selector, metadata, collectionMode);
}

void RuleFeatureSet::FeatureMetadata::add(const FeatureMetadata& other)
{
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    maxDirectAdjacentSelectors = std::max(maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors);

    HashSet<AtomicString>::const_iterator end = other.idsInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.idsInRules.begin(); it != end; ++it)
        idsInRules.add(*it);
    end = other.attrsInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.attrsInRules.begin(); it != end; ++it)
        attrsInRules.add(*it);
}

void RuleFeatureSet::FeatureMetadata::clear()
{

    idsInRules.clear();
    attrsInRules.clear();
    usesFirstLineRules = false;
    foundSiblingSelector = false;
    maxDirectAdjacentSelectors = 0;
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    for (InvalidationSetMap::const_iterator it = other.m_classInvalidationSets.begin(); it != other.m_classInvalidationSets.end(); ++it) {
        ensureClassInvalidationSet(it->key).combine(*it->value);
    }

    m_metadata.add(other.m_metadata);

    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);
}

void RuleFeatureSet::clear()
{
    m_metadata.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
}

void RuleFeatureSet::scheduleStyleInvalidationForClassChange(const SpaceSplitString& changedClasses, Element* element)
{
    if (computeInvalidationSetsForClassChange(changedClasses, element)) {
        // FIXME: remove eager calls to setNeedsStyleRecalc here, and instead reuse the invalidation tree walk.
        // This code remains for now out of conservatism about avoiding performance regressions before TargetedStyleRecalc is launched.
        element->setNeedsStyleRecalc(SubtreeStyleChange);
    }
}

void RuleFeatureSet::scheduleStyleInvalidationForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element* element)
{
    if (computeInvalidationSetsForClassChange(oldClasses, newClasses, element)) {
        // FIXME: remove eager calls to setNeedsStyleRecalc here, and instead reuse the invalidation tree walk.
        // This code remains for now out of conservatism about avoiding performance regressions before TargetedStyleRecalc is launched.
        element->setNeedsStyleRecalc(SubtreeStyleChange);
    }
}

bool RuleFeatureSet::computeInvalidationSetsForClassChange(const SpaceSplitString& changedClasses, Element* element)
{
    unsigned changedSize = changedClasses.size();
    for (unsigned i = 0; i < changedSize; ++i) {
        if (classInvalidationRequiresSubtreeRecalc(changedClasses[i]))
            return true;
        addClassToInvalidationSet(changedClasses[i], element);
    }
    return false;
}

bool RuleFeatureSet::computeInvalidationSetsForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element* element)
{
    if (!oldClasses.size())
        return computeInvalidationSetsForClassChange(newClasses, element);

    // Class vectors tend to be very short. This is faster than using a hash table.
    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldClasses.size());

    for (unsigned i = 0; i < newClasses.size(); ++i) {
        bool found = false;
        for (unsigned j = 0; j < oldClasses.size(); ++j) {
            if (newClasses[i] == oldClasses[j]) {
                // Mark each class that is still in the newClasses so we can skip doing
                // an n^2 search below when looking for removals. We can't break from
                // this loop early since a class can appear more than once.
                remainingClassBits.quickSet(j);
                found = true;
            }
        }
        // Class was added.
        if (!found) {
            if (classInvalidationRequiresSubtreeRecalc(newClasses[i]))
                return true;
            addClassToInvalidationSet(newClasses[i], element);
        }
    }

    for (unsigned i = 0; i < oldClasses.size(); ++i) {
        if (remainingClassBits.quickGet(i))
            continue;

        // Class was removed.
        if (classInvalidationRequiresSubtreeRecalc(oldClasses[i]))
            return true;
        addClassToInvalidationSet(oldClasses[i], element);
    }
    return false;
}

void RuleFeatureSet::addClassToInvalidationSet(const AtomicString& className, Element* element)
{
    if (DescendantInvalidationSet* invalidationSet = m_classInvalidationSets.get(className)) {
        ensurePendingInvalidationList(element).append(invalidationSet);
        element->setNeedsStyleInvalidation();
    }
}

RuleFeatureSet::InvalidationList& RuleFeatureSet::ensurePendingInvalidationList(Element* element)
{
    PendingInvalidationMap::AddResult addResult = m_pendingInvalidationMap.add(element, 0);
    if (addResult.isNewEntry)
        addResult.storedValue->value = new InvalidationList;
    return *addResult.storedValue->value;
}

void RuleFeatureSet::computeStyleInvalidation(Document& document)
{
    Vector<AtomicString> invalidationClasses;
    if (Element* documentElement = document.documentElement()) {
        if (documentElement->childNeedsStyleInvalidation()) {
            invalidateStyleForClassChange(documentElement, invalidationClasses, false);
        }
    }
    document.clearChildNeedsStyleInvalidation();
    m_pendingInvalidationMap.clear();
}

bool RuleFeatureSet::invalidateStyleForClassChangeOnChildren(Element* element, Vector<AtomicString>& invalidationClasses, bool foundInvalidationSet)
{
    bool someChildrenNeedStyleRecalc = false;
    for (ShadowRoot* root = element->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        for (Element* child = ElementTraversal::firstWithin(*root); child; child = ElementTraversal::nextSibling(*child)) {
            bool childRecalced = invalidateStyleForClassChange(child, invalidationClasses, foundInvalidationSet);
            someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
        }
    }
    for (Element* child = ElementTraversal::firstWithin(*element); child; child = ElementTraversal::nextSibling(*child)) {
        bool childRecalced = invalidateStyleForClassChange(child, invalidationClasses, foundInvalidationSet);
        someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
    }
    return someChildrenNeedStyleRecalc;
}

bool RuleFeatureSet::invalidateStyleForClassChange(Element* element, Vector<AtomicString>& invalidationClasses, bool foundInvalidationSet)
{
    bool thisElementNeedsStyleRecalc = false;
    int oldSize = invalidationClasses.size();

    if (element->needsStyleInvalidation()) {
        InvalidationList* invalidationList = m_pendingInvalidationMap.get(element);
        ASSERT(invalidationList);
        // FIXME: it's really only necessary to clone the render style for this element, not full style recalc.
        thisElementNeedsStyleRecalc = true;
        foundInvalidationSet = true;
        for (InvalidationList::const_iterator it = invalidationList->begin(); it != invalidationList->end(); ++it) {
            if ((*it)->wholeSubtreeInvalid()) {
                element->setNeedsStyleRecalc(SubtreeStyleChange);
                invalidationClasses.remove(oldSize, invalidationClasses.size() - oldSize);
                element->clearChildNeedsStyleInvalidation();
                return true;
            }
            (*it)->getClasses(invalidationClasses);
        }
    }

    if (element->hasClass()) {
        const SpaceSplitString& classNames = element->classNames();
        for (Vector<AtomicString>::const_iterator it = invalidationClasses.begin(); it != invalidationClasses.end(); ++it) {
            if (classNames.contains(*it)) {
                thisElementNeedsStyleRecalc = true;
                break;
            }
        }
    }

    bool someChildrenNeedStyleRecalc = false;
    // foundInvalidationSet will be true if we are in a subtree of a node with a DescendantInvalidationSet on it.
    // We need to check all nodes in the subtree of such a node.
    if (foundInvalidationSet || element->childNeedsStyleInvalidation()) {
        someChildrenNeedStyleRecalc = invalidateStyleForClassChangeOnChildren(element, invalidationClasses, foundInvalidationSet);
    }

    if (thisElementNeedsStyleRecalc) {
        element->setNeedsStyleRecalc(LocalStyleChange);
    } else if (foundInvalidationSet && someChildrenNeedStyleRecalc) {
        // Clone the RenderStyle in order to preserve correct style sharing.
        if (RenderStyle* renderStyle = element->renderStyle())
            element->renderer()->setStyle(RenderStyle::clone(renderStyle));
    }

    invalidationClasses.remove(oldSize, invalidationClasses.size() - oldSize);
    element->clearChildNeedsStyleInvalidation();
    element->clearNeedsStyleInvalidation();

    return thisElementNeedsStyleRecalc;
}

} // namespace WebCore
