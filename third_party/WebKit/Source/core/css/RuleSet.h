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

#ifndef RuleSet_h
#define RuleSet_h

#include "core/css/CSSKeyframesRule.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleFeature.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/LinkedStack.h"

namespace WebCore {

enum AddRuleFlags {
    RuleHasNoSpecialState         = 0,
    RuleHasDocumentSecurityOrigin = 1,
    RuleCanUseFastCheckSelector   = 1 << 1,
};

enum PropertyWhitelistType {
    PropertyWhitelistNone   = 0,
    PropertyWhitelistCue
};

class CSSSelector;
class MediaQueryEvaluator;
class StyleSheetContents;

struct MinimalRuleData {
    MinimalRuleData(StyleRule* rule, unsigned selectorIndex, AddRuleFlags flags)
    : m_rule(rule)
    , m_selectorIndex(selectorIndex)
    , m_flags(flags)
    {
    }

    StyleRule* m_rule;
    unsigned m_selectorIndex;
    AddRuleFlags m_flags;
};

class RuleData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RuleData(StyleRule*, unsigned selectorIndex, unsigned position, AddRuleFlags);

    unsigned position() const { return m_position; }
    StyleRule* rule() const { return m_rule; }
    const CSSSelector& selector() const { return m_rule->selectorList().selectorAt(m_selectorIndex); }
    unsigned selectorIndex() const { return m_selectorIndex; }

    bool isLastInArray() const { return m_isLastInArray; }
    void setLastInArray(bool flag) { m_isLastInArray = flag; }

    bool hasFastCheckableSelector() const { return m_hasFastCheckableSelector; }
    bool hasMultipartSelector() const { return m_hasMultipartSelector; }
    bool hasRightmostSelectorMatchingHTMLBasedOnRuleHash() const { return m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash; }
    bool containsUncommonAttributeSelector() const { return m_containsUncommonAttributeSelector; }
    unsigned specificity() const { return m_specificity; }
    unsigned linkMatchType() const { return m_linkMatchType; }
    bool hasDocumentSecurityOrigin() const { return m_hasDocumentSecurityOrigin; }
    PropertyWhitelistType propertyWhitelistType(bool isMatchingUARules = false) const { return isMatchingUARules ? PropertyWhitelistNone : static_cast<PropertyWhitelistType>(m_propertyWhitelistType); }
    // Try to balance between memory usage (there can be lots of RuleData objects) and good filtering performance.
    static const unsigned maximumIdentifierCount = 4;
    const unsigned* descendantSelectorIdentifierHashes() const { return m_descendantSelectorIdentifierHashes; }

private:
    StyleRule* m_rule;
    unsigned m_selectorIndex : 12;
    unsigned m_isLastInArray : 1; // We store an array of RuleData objects in a primitive array.
    // This number was picked fairly arbitrarily. We can probably lower it if we need to.
    // Some simple testing showed <100,000 RuleData's on large sites.
    unsigned m_position : 18;
    unsigned m_hasFastCheckableSelector : 1;
    unsigned m_specificity : 24;
    unsigned m_hasMultipartSelector : 1;
    unsigned m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash : 1;
    unsigned m_containsUncommonAttributeSelector : 1;
    unsigned m_linkMatchType : 2; //  SelectorChecker::LinkMatchMask
    unsigned m_hasDocumentSecurityOrigin : 1;
    unsigned m_propertyWhitelistType : 2;
    // Use plain array instead of a Vector to minimize memory overhead.
    unsigned m_descendantSelectorIdentifierHashes[maximumIdentifierCount];
};

struct SameSizeAsRuleData {
    void* a;
    unsigned b;
    unsigned c;
    unsigned d[4];
};

COMPILE_ASSERT(sizeof(RuleData) == sizeof(SameSizeAsRuleData), RuleData_should_stay_small);

class RuleSet {
    WTF_MAKE_NONCOPYABLE(RuleSet); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<RuleSet> create() { return adoptPtr(new RuleSet); }

    void addRulesFromSheet(StyleSheetContents*, const MediaQueryEvaluator&, AddRuleFlags = RuleHasNoSpecialState);
    void addStyleRule(StyleRule*, AddRuleFlags);
    void addRule(StyleRule*, unsigned selectorIndex, AddRuleFlags);

    const RuleFeatureSet& features() const { return m_features; }

    const RuleData* idRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_idRules.get(key); }
    const RuleData* classRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_classRules.get(key); }
    const RuleData* tagRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_tagRules.get(key); }
    const RuleData* shadowPseudoElementRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_shadowPseudoElementRules.get(key); }
    const Vector<RuleData>* linkPseudoClassRules() const { ASSERT(!m_pendingRules); return &m_linkPseudoClassRules; }
    const Vector<RuleData>* cuePseudoRules() const { ASSERT(!m_pendingRules); return &m_cuePseudoRules; }
    const Vector<RuleData>* focusPseudoClassRules() const { ASSERT(!m_pendingRules); return &m_focusPseudoClassRules; }
    const Vector<RuleData>* universalRules() const { ASSERT(!m_pendingRules); return &m_universalRules; }
    const Vector<StyleRulePage*>& pageRules() const { ASSERT(!m_pendingRules); return m_pageRules; }
    const Vector<StyleRuleViewport*>& viewportRules() const { ASSERT(!m_pendingRules); return m_viewportRules; }
    const Vector<StyleRuleFontFace*>& fontFaceRules() const { return m_fontFaceRules; }
    const Vector<StyleRuleKeyframes*>& keyframesRules() const { return m_keyframesRules; }
    const Vector<MinimalRuleData>& treeBoundaryCrossingRules() const { return m_treeBoundaryCrossingRules; }
    const Vector<MinimalRuleData>& shadowDistributedRules() const { return m_shadowDistributedRules; }
    const MediaQueryResultList& viewportDependentMediaQueryResults() const { return m_viewportDependentMediaQueryResults; }

    unsigned ruleCount() const { return m_ruleCount; }

    void compactRulesIfNeeded()
    {
        if (!m_pendingRules)
            return;
        compactRules();
    }

#ifndef NDEBUG
    void show();
#endif

    struct RuleSetSelectorPair {
        RuleSetSelectorPair(const CSSSelector* selector, PassOwnPtr<RuleSet> ruleSet) : selector(selector), ruleSet(ruleSet) { }
        RuleSetSelectorPair(const RuleSetSelectorPair& rs) : selector(rs.selector), ruleSet(const_cast<RuleSetSelectorPair*>(&rs)->ruleSet.release()) { }

        const CSSSelector* selector;
        OwnPtr<RuleSet> ruleSet;
    };

private:
    typedef HashMap<AtomicString, OwnPtr<LinkedStack<RuleData> > > PendingRuleMap;
    typedef HashMap<AtomicString, OwnPtr<RuleData> > CompactRuleMap;

    RuleSet()
        : m_ruleCount(0)
    {
    }

    void addToRuleSet(const AtomicString& key, PendingRuleMap&, const RuleData&);
    void addPageRule(StyleRulePage*);
    void addViewportRule(StyleRuleViewport*);
    void addFontFaceRule(StyleRuleFontFace*);
    void addKeyframesRule(StyleRuleKeyframes*);

    void addChildRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> >&, const MediaQueryEvaluator& medium, AddRuleFlags);
    bool findBestRuleSetAndAdd(const CSSSelector&, RuleData&);

    void compactRules();
    static void compactPendingRules(PendingRuleMap&, CompactRuleMap&);

    struct PendingRuleMaps {
        PendingRuleMap idRules;
        PendingRuleMap classRules;
        PendingRuleMap tagRules;
        PendingRuleMap shadowPseudoElementRules;
    };

    PendingRuleMaps* ensurePendingRules()
    {
        if (!m_pendingRules)
            m_pendingRules = adoptPtr(new PendingRuleMaps);
        return m_pendingRules.get();
    }

    CompactRuleMap m_idRules;
    CompactRuleMap m_classRules;
    CompactRuleMap m_tagRules;
    CompactRuleMap m_shadowPseudoElementRules;
    Vector<RuleData> m_linkPseudoClassRules;
    Vector<RuleData> m_cuePseudoRules;
    Vector<RuleData> m_focusPseudoClassRules;
    Vector<RuleData> m_universalRules;
    RuleFeatureSet m_features;
    Vector<StyleRulePage*> m_pageRules;
    Vector<StyleRuleViewport*> m_viewportRules;
    Vector<StyleRuleFontFace*> m_fontFaceRules;
    Vector<StyleRuleKeyframes*> m_keyframesRules;
    Vector<MinimalRuleData> m_treeBoundaryCrossingRules;
    Vector<MinimalRuleData> m_shadowDistributedRules;

    MediaQueryResultList m_viewportDependentMediaQueryResults;

    unsigned m_ruleCount;
    OwnPtr<PendingRuleMaps> m_pendingRules;

#ifndef NDEBUG
    Vector<RuleData> m_allRules;
#endif
};

} // namespace WebCore

#endif // RuleSet_h
