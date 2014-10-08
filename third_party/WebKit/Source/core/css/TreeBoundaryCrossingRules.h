/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#ifndef TreeBoundaryCrossingRules_h
#define TreeBoundaryCrossingRules_h

#include "core/css/RuleSet.h"
#include "core/dom/DocumentOrderedList.h"

#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class CSSStyleSheet;
class ContainerNode;
class ElementRuleCollector;
class RuleFeatureSet;

class TreeBoundaryCrossingRules final {
    DISALLOW_ALLOCATION();
public:
    void addTreeBoundaryCrossingRules(const RuleSet&, CSSStyleSheet*, unsigned sheetIndex, ContainerNode&);

    void reset(const ContainerNode* scopingNode);
    void collectFeaturesTo(RuleFeatureSet&);
    void collectTreeBoundaryCrossingRules(Element*, ElementRuleCollector&, bool includeEmptyRules);

    void trace(Visitor*);

private:
    size_t size() const { return m_scopingNodes.size(); }
    class RuleSubSet final : public NoBaseWillBeGarbageCollected<RuleSubSet> {
    public:
        static PassOwnPtrWillBeRawPtr<RuleSubSet> create(CSSStyleSheet* sheet, unsigned index, PassOwnPtrWillBeRawPtr<RuleSet> rules)
        {
            return adoptPtrWillBeNoop(new RuleSubSet(sheet, index, rules));
        }

        CSSStyleSheet* parentStyleSheet;
        unsigned parentIndex;
        OwnPtrWillBeMember<RuleSet> ruleSet;

        void trace(Visitor*);

    private:
        RuleSubSet(CSSStyleSheet* sheet, unsigned index, PassOwnPtrWillBeRawPtr<RuleSet> rules)
            : parentStyleSheet(sheet)
            , parentIndex(index)
            , ruleSet(rules)
        {
        }
    };
    typedef WillBeHeapVector<OwnPtrWillBeMember<RuleSubSet> > CSSStyleSheetRuleSubSet;
    void collectFeaturesFromRuleSubSet(CSSStyleSheetRuleSubSet*, RuleFeatureSet&);

    DocumentOrderedList m_scopingNodes;
    typedef WillBeHeapHashMap<RawPtrWillBeMember<const ContainerNode>, OwnPtrWillBeMember<CSSStyleSheetRuleSubSet> > TreeBoundaryCrossingRuleSetMap;
    TreeBoundaryCrossingRuleSetMap m_treeBoundaryCrossingRuleSetMap;
};

} // namespace blink

#endif // TreeBoundaryCrossingRules_h
