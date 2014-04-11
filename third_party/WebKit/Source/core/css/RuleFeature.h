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

#include "core/css/invalidation/StyleInvalidator.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class CSSSelector;
class CSSSelectorList;
class DescendantInvalidationSet;
class Document;
class Node;
class QualifiedName;
class RuleData;
class ShadowRoot;
class SpaceSplitString;
class StyleRule;

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
    ~RuleFeatureSet();

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
        return m_attributeInvalidationSets.contains(attributeName);
    }

    inline bool hasSelectorForClass(const AtomicString& classValue) const
    {
        ASSERT(!classValue.isEmpty());
        return m_classInvalidationSets.contains(classValue);
    }

    inline bool hasSelectorForId(const AtomicString& idValue) const
    {
        return m_idInvalidationSets.contains(idValue);
    }

    void scheduleStyleInvalidationForClassChange(const SpaceSplitString& changedClasses, Element&);
    void scheduleStyleInvalidationForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element&);

    void scheduleStyleInvalidationForAttributeChange(const QualifiedName& attributeName, Element&);

    void scheduleStyleInvalidationForIdChange(const AtomicString& oldId, const AtomicString& newId, Element&);

    bool hasIdsInSelectors() const
    {
        return m_idInvalidationSets.size() > 0;
    }

    // Marks the given attribute name as "appearing in a selector". Used for
    // CSS properties such as content: ... attr(...) ...
    // FIXME: record these internally to this class instead calls from StyleResolver to here.
    void addContentAttr(const AtomicString& attributeName);

    StyleInvalidator& styleInvalidator();

    Vector<RuleFeature> siblingRules;
    Vector<RuleFeature> uncommonAttributeRules;

private:
    typedef HashMap<AtomicString, RefPtr<DescendantInvalidationSet> > InvalidationSetMap;

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
    };

    enum InvalidationSetMode {
        AddFeatures,
        UseLocalStyleChange,
        UseSubtreeStyleChange
    };

    static InvalidationSetMode invalidationSetModeForSelector(const CSSSelector&);

    void collectFeaturesFromSelector(const CSSSelector&, FeatureMetadata&, InvalidationSetMode);
    void collectFeaturesFromSelectorList(const CSSSelectorList*, FeatureMetadata&, InvalidationSetMode);

    DescendantInvalidationSet& ensureClassInvalidationSet(const AtomicString& className);
    DescendantInvalidationSet& ensureAttributeInvalidationSet(const AtomicString& attributeName);
    DescendantInvalidationSet& ensureIdInvalidationSet(const AtomicString& attributeName);
    DescendantInvalidationSet* invalidationSetForSelector(const CSSSelector&);

    InvalidationSetMode updateInvalidationSets(const CSSSelector&);

    struct InvalidationSetFeatures {
        InvalidationSetFeatures() : customPseudoElement(false) { }
        Vector<AtomicString> classes;
        Vector<AtomicString> attributes;
        AtomicString id;
        AtomicString tagName;
        bool customPseudoElement;
    };

    static void extractInvalidationSetFeature(const CSSSelector&, InvalidationSetFeatures&);
    const CSSSelector* extractInvalidationSetFeatures(const CSSSelector&, InvalidationSetFeatures&);
    void addFeaturesToInvalidationSets(const CSSSelector&, const InvalidationSetFeatures&, bool wholeSubtree);

    void addClassToInvalidationSet(const AtomicString& className, Element&);

    FeatureMetadata m_metadata;
    InvalidationSetMap m_classInvalidationSets;
    InvalidationSetMap m_attributeInvalidationSets;
    InvalidationSetMap m_idInvalidationSets;
    bool m_targetedStyleRecalcEnabled;
    StyleInvalidator m_styleInvalidator;
};


} // namespace WebCore

#endif // RuleFeature_h
