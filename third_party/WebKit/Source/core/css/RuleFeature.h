/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef RuleFeature_h
#define RuleFeature_h

#include "core/css/analyzer/DescendantInvalidationSet.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class Document;
class ShadowRoot;
class StyleRule;
class CSSSelector;
class CSSSelectorList;
class RuleData;
class SpaceSplitString;

struct RuleFeature {
    RuleFeature(StyleRule* rule, unsigned selectorIndex, bool hasDocumentSecurityOrigin)
        : rule(rule)
        , selectorIndex(selectorIndex)
        , hasDocumentSecurityOrigin(hasDocumentSecurityOrigin)
    {
    }
    StyleRule* rule;
    unsigned selectorIndex;
    bool hasDocumentSecurityOrigin;
};

class RuleFeatureSet {
public:
    RuleFeatureSet();

    void add(const RuleFeatureSet&);
    void clear();

    void collectFeaturesFromSelector(const CSSSelector&);
    void collectFeaturesFromRuleData(const RuleData&);

    bool usesSiblingRules() const { return !siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_metadata.usesFirstLineRules; }

    unsigned maxDirectAdjacentSelectors() const { return m_metadata.maxDirectAdjacentSelectors; }
    void setMaxDirectAdjacentSelectors(unsigned value)  { m_metadata.maxDirectAdjacentSelectors = std::max(value, m_metadata.maxDirectAdjacentSelectors); }

    inline bool hasSelectorForAttribute(const AtomicString& attributeName) const
    {
        ASSERT(!attributeName.isEmpty());
        return m_metadata.attrsInRules.contains(attributeName);
    }

    inline bool hasSelectorForClass(const AtomicString& classValue) const
    {
        ASSERT(!classValue.isEmpty());
        return m_classInvalidationSets.get(classValue);

    }

    inline bool hasSelectorForId(const AtomicString& idValue) const
    {
        return m_metadata.idsInRules.contains(idValue);
    }

    void scheduleStyleInvalidationForClassChange(const SpaceSplitString& changedClasses, Element*);
    void scheduleStyleInvalidationForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element*);

    void computeStyleInvalidation(Document&);

    int hasIdsInSelectors() const
    {
        return m_metadata.idsInRules.size() > 0;
    }

    // Marks the given attribute name as "appearing in a selector". Used for
    // CSS properties such as content: ... attr(...) ...
    void addAttributeInASelector(const AtomicString& attributeName);

    Vector<RuleFeature> siblingRules;
    Vector<RuleFeature> uncommonAttributeRules;

private:
    typedef HashMap<AtomicString, RefPtr<DescendantInvalidationSet> > InvalidationSetMap;
    typedef Vector<RefPtr<DescendantInvalidationSet> > InvalidationList;
    typedef HashMap<Element*, OwnPtr<InvalidationList> > PendingInvalidationMap;
    struct FeatureMetadata {
        FeatureMetadata()
            : usesFirstLineRules(false)
            , foundSiblingSelector(false)
            , maxDirectAdjacentSelectors(0)
        { }
        void add(const FeatureMetadata& other);
        void clear();

        bool usesFirstLineRules;
        bool foundSiblingSelector;
        unsigned maxDirectAdjacentSelectors;
        HashSet<AtomicString> idsInRules;
        HashSet<AtomicString> attrsInRules;
    };

    // These return true if setNeedsStyleRecalc() should be run on the Element, as a fallback.
    bool computeInvalidationSetsForClassChange(const SpaceSplitString& changedClasses, Element*);
    bool computeInvalidationSetsForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element*);

    enum SelectorFeatureCollectionMode {
        ProcessClasses,
        DontProcessClasses
    };

    void collectFeaturesFromSelector(const CSSSelector&, FeatureMetadata&, SelectorFeatureCollectionMode processClasses);
    void collectFeaturesFromSelectorList(const CSSSelectorList*, FeatureMetadata&, SelectorFeatureCollectionMode processClasses);

    bool classInvalidationRequiresSubtreeRecalc(const AtomicString& className);
    DescendantInvalidationSet& ensureClassInvalidationSet(const AtomicString& className);
    bool updateClassInvalidationSets(const CSSSelector&);

    void addClassToInvalidationSet(const AtomicString& className, Element*);

    bool invalidateStyleForClassChange(Element*, Vector<AtomicString>&, bool foundInvalidationSet);
    bool invalidateStyleForClassChangeOnChildren(Element*, Vector<AtomicString>& invalidationClasses, bool foundInvalidationSet);

    InvalidationList& ensurePendingInvalidationList(Element*);

    FeatureMetadata m_metadata;
    InvalidationSetMap m_classInvalidationSets;
    PendingInvalidationMap m_pendingInvalidationMap;

    bool m_targetedStyleRecalcEnabled;
};

} // namespace WebCore

#endif // RuleFeature_h
