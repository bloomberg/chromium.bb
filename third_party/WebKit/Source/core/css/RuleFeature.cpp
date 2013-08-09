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
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"

namespace WebCore {

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector* selector)
{
    if (selector->m_match == CSSSelector::Id)
        idsInRules.add(selector->value().impl());
    else if (selector->m_match == CSSSelector::Class)
        classesInRules.add(selector->value().impl());
    else if (selector->isAttributeSelector())
        attrsInRules.add(selector->attribute().localName().impl());
    switch (selector->pseudoType()) {
    case CSSSelector::PseudoFirstLine:
        m_usesFirstLineRules = true;
        break;
    case CSSSelector::PseudoBefore:
    case CSSSelector::PseudoAfter:
        m_usesBeforeAfterRules = true;
        break;
    case CSSSelector::PseudoPart:
        attrsInRules.add(HTMLNames::partAttr.localName().impl());
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
    m_usesBeforeAfterRules = m_usesBeforeAfterRules || other.m_usesBeforeAfterRules;
}

void RuleFeatureSet::clear()
{
    idsInRules.clear();
    classesInRules.clear();
    attrsInRules.clear();
    siblingRules.clear();
    uncommonAttributeRules.clear();
    m_usesFirstLineRules = false;
    m_usesBeforeAfterRules = false;
}

} // namespace WebCore
