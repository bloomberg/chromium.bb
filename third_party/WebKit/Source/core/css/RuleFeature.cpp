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

namespace WebCore {

static bool isSkippableComponentForInvalidation(const CSSSelector* selector)
{
    if (selector->matchesPseudoElement() || selector->pseudoType() == CSSSelector::PseudoHost)
        return false;
    return true;
}

// This method is somewhat conservative in what it acceptss.
static bool supportsClassDescendantInvalidation(const CSSSelector* selector)
{
    bool foundDescendantRelation = false;
    bool foundAncestorIdent = false;
    bool foundIdent = false;
    for (const CSSSelector* component = selector; component; component = component->tagHistory()) {

        // FIXME: We should allow pseudo elements, but we need to change how they hook
        // into recalcStyle by moving them to recalcOwnStyle instead of recalcChildStyle.

        if (component->m_match == CSSSelector::Tag
            || component->m_match == CSSSelector::Id
            || component->m_match == CSSSelector::Class) {
            if (!foundDescendantRelation)
                foundIdent = true;
            else
                foundAncestorIdent = true;
        } else if (!isSkippableComponentForInvalidation(component)) {
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
    return foundDescendantRelation && foundAncestorIdent && foundIdent;
}

void extractClassIdOrTag(const CSSSelector& selector, HashSet<AtomicString>& classes, AtomicString& id, AtomicString& tagName)
{
    if (selector.m_match == CSSSelector::Tag)
        tagName = selector.tagQName().localName();
    else if (selector.m_match == CSSSelector::Id)
        id = selector.value();
    else if (selector.m_match == CSSSelector::Class)
        classes.add(selector.value());
}

bool RuleFeatureSet::updateClassInvalidationSets(const CSSSelector* selector)
{
    if (!selector)
        return false;
    if (!supportsClassDescendantInvalidation(selector))
        return false;

    HashSet<AtomicString> classes;
    AtomicString id;
    AtomicString tagName;

    const CSSSelector* lastSelector = selector;
    for (; lastSelector->relation() == CSSSelector::SubSelector; lastSelector = lastSelector->tagHistory()) {
        extractClassIdOrTag(*selector, classes, id, tagName);
    }
    extractClassIdOrTag(*selector, classes, id, tagName);

    for ( ; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Class) {
            DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(selector->value());
            if (!id.isEmpty())
                invalidationSet.addId(id);
            if (!tagName.isEmpty())
                invalidationSet.addTagName(tagName);
            for (HashSet<AtomicString>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
                invalidationSet.addClass(*it);
            }
        }
    }
    return true;
}

void RuleFeatureSet::collectFeaturesFromRuleData(const RuleData& ruleData)
{
    bool foundSiblingSelector = false;
    unsigned maxDirectAdjacentSelectors = 0;
    for (const CSSSelector* selector = ruleData.selector(); selector; selector = selector->tagHistory()) {
        collectFeaturesFromSelector(selector);

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                // FIXME: Shouldn't this be checking subSelector->isSiblingSelector()?
                if (!foundSiblingSelector && selector->isSiblingSelector())
                    foundSiblingSelector = true;
                if (subSelector->isDirectAdjacentSelector())
                    maxDirectAdjacentSelectors++;
                collectFeaturesFromSelector(subSelector);
            }
        } else {
            if (!foundSiblingSelector && selector->isSiblingSelector())
                foundSiblingSelector = true;
            if (selector->isDirectAdjacentSelector())
                maxDirectAdjacentSelectors++;
        }
    }
    if (RuntimeEnabledFeatures::targetedStyleRecalcEnabled()) {
        bool selectorUsesClassInvalidationSet = updateClassInvalidationSets(ruleData.selector());
        if (!selectorUsesClassInvalidationSet) {
            for (HashSet<AtomicString>::const_iterator it = classesInRules.begin(); it != classesInRules.end(); ++it) {
                DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(*it);
                invalidationSet.setWholeSubtreeInvalid();
            }
        }
    }
    setMaxDirectAdjacentSelectors(maxDirectAdjacentSelectors);
    if (foundSiblingSelector)
        siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
}

DescendantInvalidationSet& RuleFeatureSet::ensureClassInvalidationSet(const AtomicString& className)
{
    InvalidationSetMap::AddResult addResult = m_classInvalidationSets.add(className, 0);
    if (addResult.isNewEntry)
        addResult.iterator->value = DescendantInvalidationSet::create();
    return *addResult.iterator->value;
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector* selector)
{
    if (selector->m_match == CSSSelector::Id)
        idsInRules.add(selector->value());
    else if (selector->m_match == CSSSelector::Class)
        classesInRules.add(selector->value());
    else if (selector->isAttributeSelector())
        attrsInRules.add(selector->attribute().localName());
    switch (selector->pseudoType()) {
    case CSSSelector::PseudoFirstLine:
        m_usesFirstLineRules = true;
        break;
    case CSSSelector::PseudoHost:
        collectFeaturesFromSelectorList(selector->selectorList());
        break;
    default:
        break;
    }
}

void RuleFeatureSet::collectFeaturesFromSelectorList(const CSSSelectorList* selectorList)
{
    if (!selectorList)
        return;

    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(selector)) {
        for (const CSSSelector* subSelector = selector; subSelector; subSelector = subSelector->tagHistory())
            collectFeaturesFromSelector(subSelector);
    }
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    for (InvalidationSetMap::const_iterator it = other.m_classInvalidationSets.begin(); it != other.m_classInvalidationSets.end(); ++it) {
        ensureClassInvalidationSet(it->key).combine(*it->value);
    }

    HashSet<AtomicString>::const_iterator end = other.idsInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.idsInRules.begin(); it != end; ++it)
        idsInRules.add(*it);
    end = other.classesInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.classesInRules.begin(); it != end; ++it)
        classesInRules.add(*it);
    end = other.attrsInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.attrsInRules.begin(); it != end; ++it)
        attrsInRules.add(*it);
    siblingRules.append(other.siblingRules);
    uncommonAttributeRules.append(other.uncommonAttributeRules);
    m_usesFirstLineRules = m_usesFirstLineRules || other.m_usesFirstLineRules;
    m_maxDirectAdjacentSelectors = std::max(m_maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors());
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    classesInRules.clear();
    attrsInRules.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
    m_usesFirstLineRules = false;
    m_maxDirectAdjacentSelectors = 0;
}

} // namespace WebCore
