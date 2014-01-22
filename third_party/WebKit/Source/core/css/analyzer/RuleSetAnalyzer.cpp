/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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
#include "core/css/analyzer/RuleSetAnalyzer.h"

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/analyzer/DescendantInvalidationSet.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringHash.h"


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

RuleSetAnalyzer::RuleSetAnalyzer()
{
}

bool RuleSetAnalyzer::updateClassInvalidationSets(const CSSSelector* selector)
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

PassRefPtr<RuleSetAnalyzer> RuleSetAnalyzer::create()
{
    return adoptRef(new RuleSetAnalyzer);
}

void RuleSetAnalyzer::collectFeaturesFromRuleData(RuleFeatureSet& features, const RuleData& ruleData)
{
    bool foundSiblingSelector = false;
    unsigned maxDirectAdjacentSelectors = 0;
    for (const CSSSelector* selector = ruleData.selector(); selector; selector = selector->tagHistory()) {
        features.collectFeaturesFromSelector(selector);

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                // FIXME: Shouldn't this be checking subSelector->isSiblingSelector()?
                if (!foundSiblingSelector && selector->isSiblingSelector())
                    foundSiblingSelector = true;
                if (subSelector->isDirectAdjacentSelector())
                    maxDirectAdjacentSelectors++;
                features.collectFeaturesFromSelector(subSelector);
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
            for (HashSet<AtomicString>::const_iterator it = features.classesInRules.begin(); it != features.classesInRules.end(); ++it) {
                DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(*it);
                invalidationSet.setWholeSubtreeInvalid();
            }
        }
    }
    features.setMaxDirectAdjacentSelectors(maxDirectAdjacentSelectors);
    if (foundSiblingSelector)
        features.siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        features.uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
}

DescendantInvalidationSet& RuleSetAnalyzer::ensureClassInvalidationSet(const AtomicString& className)
{
    InvalidationSetMap::AddResult addResult = m_classInvalidationSets.add(className, 0);
    if (addResult.isNewEntry)
        addResult.iterator->value = DescendantInvalidationSet::create();
    return *addResult.iterator->value;
}

void RuleSetAnalyzer::combine(const RuleSetAnalyzer& other)
{
    for (InvalidationSetMap::const_iterator it = other.m_classInvalidationSets.begin(); it != other.m_classInvalidationSets.end(); ++it) {
        ensureClassInvalidationSet(it->key).combine(*it->value);
    }
}

} // namespace WebCore
