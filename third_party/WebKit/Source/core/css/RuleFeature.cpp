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
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "wtf/BitVector.h"

namespace WebCore {

static bool isSkippableComponentForInvalidation(const CSSSelector& selector)
{
    if (selector.m_match == CSSSelector::Tag
        || selector.m_match == CSSSelector::Id
        || selector.isAttributeSelector())
        return true;
    if (selector.m_match == CSSSelector::PseudoElement) {
        switch (selector.pseudoType()) {
        case CSSSelector::PseudoBefore:
        case CSSSelector::PseudoAfter:
        case CSSSelector::PseudoBackdrop:
        case CSSSelector::PseudoShadow:
            return true;
        default:
            return selector.isCustomPseudoElement();
        }
    }
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
}

// This method is somewhat conservative in what it accepts.
RuleFeatureSet::InvalidationSetMode RuleFeatureSet::invalidationSetModeForSelector(const CSSSelector& selector)
{
    bool foundDescendantRelation = false;
    bool foundIdent = false;
    for (const CSSSelector* component = &selector; component; component = component->tagHistory()) {

        // FIXME: next up: Tag.
        if (component->m_match == CSSSelector::Class || component->m_match == CSSSelector::Id || component->isAttributeSelector() || component->isCustomPseudoElement()) {
            if (!foundDescendantRelation)
                foundIdent = true;
        } else if (component->pseudoType() == CSSSelector::PseudoHost || component->pseudoType() == CSSSelector::PseudoAny) {
            if (const CSSSelectorList* selectorList = component->selectorList()) {
                for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector)) {
                    InvalidationSetMode hostMode = invalidationSetModeForSelector(*selector);
                    if (hostMode == UseSubtreeStyleChange)
                        return foundDescendantRelation ? UseLocalStyleChange : UseSubtreeStyleChange;
                    if (!foundDescendantRelation && hostMode == AddFeatures)
                        foundIdent = true;
                }
            }
        } else if (!isSkippableComponentForInvalidation(*component)) {
            return foundDescendantRelation ? UseLocalStyleChange : UseSubtreeStyleChange;
        }
        switch (component->relation()) {
        case CSSSelector::Descendant:
        case CSSSelector::Child:
        case CSSSelector::ShadowPseudo:
        case CSSSelector::ShadowDeep:
            foundDescendantRelation = true;
            // Fall through!
        case CSSSelector::SubSelector:
            continue;
        default:
            return UseLocalStyleChange;
        }
    }
    return foundIdent ? AddFeatures : UseLocalStyleChange;
}

void RuleFeatureSet::extractInvalidationSetFeature(const CSSSelector& selector, InvalidationSetFeatures& features)
{
    if (selector.m_match == CSSSelector::Tag)
        features.tagName = selector.tagQName().localName();
    else if (selector.m_match == CSSSelector::Id)
        features.id = selector.value();
    else if (selector.m_match == CSSSelector::Class)
        features.classes.append(selector.value());
    else if (selector.isAttributeSelector())
        features.attributes.append(selector.attribute().localName());
    else if (selector.isCustomPseudoElement())
        features.customPseudoElement = true;
}

RuleFeatureSet::RuleFeatureSet()
    : m_targetedStyleRecalcEnabled(RuntimeEnabledFeatures::targetedStyleRecalcEnabled())
{
}

DescendantInvalidationSet* RuleFeatureSet::invalidationSetForSelector(const CSSSelector& selector)
{
    if (selector.m_match == CSSSelector::Class)
        return &ensureClassInvalidationSet(selector.value());
    if (selector.isAttributeSelector())
        return &ensureAttributeInvalidationSet(selector.attribute().localName());
    if (selector.m_match == CSSSelector::Id)
        return &ensureIdInvalidationSet(selector.value());
    return 0;
}

RuleFeatureSet::InvalidationSetMode RuleFeatureSet::updateInvalidationSets(const CSSSelector& selector)
{
    InvalidationSetMode mode = invalidationSetModeForSelector(selector);
    if (mode != AddFeatures)
        return mode;

    InvalidationSetFeatures features;
    const CSSSelector* current = extractInvalidationSetFeatures(selector, features);
    if (current)
        current = current->tagHistory();

    if (current)
        addFeaturesToInvalidationSets(*current, features);
    return AddFeatures;
}

const CSSSelector* RuleFeatureSet::extractInvalidationSetFeatures(const CSSSelector& selector, InvalidationSetFeatures& features)
{
    const CSSSelector* lastSelector = &selector;
    for (; lastSelector; lastSelector = lastSelector->tagHistory()) {
        extractInvalidationSetFeature(*lastSelector, features);
        // Initialize the entry in the invalidation set map, if supported.
        invalidationSetForSelector(*lastSelector);
        if (lastSelector->pseudoType() == CSSSelector::PseudoHost || lastSelector->pseudoType() == CSSSelector::PseudoAny) {
            if (const CSSSelectorList* selectorList = lastSelector->selectorList()) {
                for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector))
                    extractInvalidationSetFeatures(*selector, features);
            }
        }

        if (lastSelector->relation() != CSSSelector::SubSelector)
            break;
    }
    return lastSelector;
}

void RuleFeatureSet::addFeaturesToInvalidationSets(const CSSSelector& selector, const InvalidationSetFeatures& features)
{
    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        if (DescendantInvalidationSet* invalidationSet = invalidationSetForSelector(*current)) {
            if (!features.id.isEmpty())
                invalidationSet->addId(features.id);
            if (!features.tagName.isEmpty())
                invalidationSet->addTagName(features.tagName);
            for (Vector<AtomicString>::const_iterator it = features.classes.begin(); it != features.classes.end(); ++it)
                invalidationSet->addClass(*it);
            for (Vector<AtomicString>::const_iterator it = features.attributes.begin(); it != features.attributes.end(); ++it)
                invalidationSet->addAttribute(*it);
            if (features.customPseudoElement)
                invalidationSet->setCustomPseudoInvalid();
        } else if (current->pseudoType() == CSSSelector::PseudoHost || current->pseudoType() == CSSSelector::PseudoAny) {
            if (const CSSSelectorList* selectorList = current->selectorList()) {
                for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector))
                    addFeaturesToInvalidationSets(*selector, features);
            }
        }
    }
}

void RuleFeatureSet::addContentAttr(const AtomicString& attributeName)
{
    DescendantInvalidationSet& invalidationSet = ensureAttributeInvalidationSet(attributeName);
    invalidationSet.setWholeSubtreeInvalid();
}

void RuleFeatureSet::collectFeaturesFromRuleData(const RuleData& ruleData)
{
    FeatureMetadata metadata;
    InvalidationSetMode mode = UseSubtreeStyleChange;
    if (m_targetedStyleRecalcEnabled)
        mode = updateInvalidationSets(ruleData.selector());

    collectFeaturesFromSelector(ruleData.selector(), metadata, mode);
    m_metadata.add(metadata);

    if (metadata.foundSiblingSelector)
        siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
}

DescendantInvalidationSet& RuleFeatureSet::ensureClassInvalidationSet(const AtomicString& className)
{
    InvalidationSetMap::AddResult addResult = m_classInvalidationSets.add(className, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = DescendantInvalidationSet::create();
    return *addResult.storedValue->value;
}

DescendantInvalidationSet& RuleFeatureSet::ensureAttributeInvalidationSet(const AtomicString& attributeName)
{
    InvalidationSetMap::AddResult addResult = m_attributeInvalidationSets.add(attributeName, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = DescendantInvalidationSet::create();
    return *addResult.storedValue->value;
}

DescendantInvalidationSet& RuleFeatureSet::ensureIdInvalidationSet(const AtomicString& id)
{
    InvalidationSetMap::AddResult addResult = m_idInvalidationSets.add(id, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = DescendantInvalidationSet::create();
    return *addResult.storedValue->value;
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector)
{
    collectFeaturesFromSelector(selector, m_metadata, UseSubtreeStyleChange);
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector, RuleFeatureSet::FeatureMetadata& metadata, InvalidationSetMode mode)
{
    unsigned maxDirectAdjacentSelectors = 0;

    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        if (mode != AddFeatures && (current->m_match == CSSSelector::Class || current->m_match == CSSSelector::Id || current->isAttributeSelector())) {
            DescendantInvalidationSet* invalidationSet = invalidationSetForSelector(*current);
            ASSERT(invalidationSet);
            if (mode == UseSubtreeStyleChange)
                invalidationSet->setWholeSubtreeInvalid();
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

        collectFeaturesFromSelectorList(current->selectorList(), metadata, mode);

        if (mode == UseLocalStyleChange && current->relation() != CSSSelector::SubSelector)
            mode = UseSubtreeStyleChange;
    }

    ASSERT(!maxDirectAdjacentSelectors);
}

void RuleFeatureSet::collectFeaturesFromSelectorList(const CSSSelectorList* selectorList, RuleFeatureSet::FeatureMetadata& metadata, InvalidationSetMode mode)
{
    if (!selectorList)
        return;

    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector))
        collectFeaturesFromSelector(*selector, metadata, mode);
}

void RuleFeatureSet::FeatureMetadata::add(const FeatureMetadata& other)
{
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    maxDirectAdjacentSelectors = std::max(maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors);
}

void RuleFeatureSet::FeatureMetadata::clear()
{
    usesFirstLineRules = false;
    foundSiblingSelector = false;
    maxDirectAdjacentSelectors = 0;
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    for (InvalidationSetMap::const_iterator it = other.m_classInvalidationSets.begin(); it != other.m_classInvalidationSets.end(); ++it)
        ensureClassInvalidationSet(it->key).combine(*it->value);
    for (InvalidationSetMap::const_iterator it = other.m_attributeInvalidationSets.begin(); it != other.m_attributeInvalidationSets.end(); ++it)
        ensureAttributeInvalidationSet(it->key).combine(*it->value);
    for (InvalidationSetMap::const_iterator it = other.m_idInvalidationSets.begin(); it != other.m_idInvalidationSets.end(); ++it)
        ensureIdInvalidationSet(it->key).combine(*it->value);

    m_metadata.add(other.m_metadata);

    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);
}

void RuleFeatureSet::clear()
{
    siblingRules.clear();
    uncommonAttributeRules.clear();
    m_metadata.clear();
    m_classInvalidationSets.clear();
    m_attributeInvalidationSets.clear();
    m_idInvalidationSets.clear();
    m_styleInvalidator.clearPendingInvalidations();
}

void RuleFeatureSet::scheduleStyleInvalidationForClassChange(const SpaceSplitString& changedClasses, Element& element)
{
    unsigned changedSize = changedClasses.size();
    for (unsigned i = 0; i < changedSize; ++i) {
        addClassToInvalidationSet(changedClasses[i], element);
    }
}

void RuleFeatureSet::scheduleStyleInvalidationForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element& element)
{
    if (!oldClasses.size())
        scheduleStyleInvalidationForClassChange(newClasses, element);

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
        if (!found)
            addClassToInvalidationSet(newClasses[i], element);
    }

    for (unsigned i = 0; i < oldClasses.size(); ++i) {
        if (remainingClassBits.quickGet(i))
            continue;
        // Class was removed.
        addClassToInvalidationSet(oldClasses[i], element);
    }
}

void RuleFeatureSet::scheduleStyleInvalidationForAttributeChange(const QualifiedName& attributeName, Element& element)
{
    if (RefPtr<DescendantInvalidationSet> invalidationSet = m_attributeInvalidationSets.get(attributeName.localName()))
        m_styleInvalidator.scheduleInvalidation(invalidationSet, element);
}

void RuleFeatureSet::scheduleStyleInvalidationForIdChange(const AtomicString& oldId, const AtomicString& newId, Element& element)
{
    if (!oldId.isEmpty()) {
        if (RefPtr<DescendantInvalidationSet> invalidationSet = m_idInvalidationSets.get(oldId))
            m_styleInvalidator.scheduleInvalidation(invalidationSet, element);
    }
    if (!newId.isEmpty()) {
        if (RefPtr<DescendantInvalidationSet> invalidationSet = m_idInvalidationSets.get(newId))
            m_styleInvalidator.scheduleInvalidation(invalidationSet, element);
    }
}

void RuleFeatureSet::addClassToInvalidationSet(const AtomicString& className, Element& element)
{
    if (RefPtr<DescendantInvalidationSet> invalidationSet = m_classInvalidationSets.get(className))
        m_styleInvalidator.scheduleInvalidation(invalidationSet, element);
}

StyleInvalidator& RuleFeatureSet::styleInvalidator()
{
    return m_styleInvalidator;
}

} // namespace WebCore
