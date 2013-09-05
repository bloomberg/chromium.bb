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

#ifndef StyleResolver_h
#define StyleResolver_h

#include "core/animation/KeyframeAnimationEffect.h"
#include "core/css/DocumentRuleSets.h"
#include "core/css/InspectorCSSOMWrappers.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorFilter.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/css/resolver/MatchedPropertiesCache.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/StyleResourceLoader.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSAnimationUpdate;
class CSSFontSelector;
class CSSRuleList;
class CSSSelector;
class CSSStyleSheet;
class CSSValue;
class ContainerNode;
class Document;
class DocumentTimeline;
class Element;
class ElementRuleCollector;
class KeyframeList;
class KeyframeValue;
class MediaQueryEvaluator;
class MediaQueryExp;
class MediaQueryResult;
class RenderRegion;
class RuleData;
class Settings;
class StyleKeyframe;
class StylePropertySet;
class StyleRule;
class StyleRuleKeyframes;
class StyleRulePage;
class ViewportStyleResolver;

struct MatchResult;

enum StyleSharingBehavior {
    AllowStyleSharing,
    DisallowStyleSharing,
};

// MatchOnlyUserAgentRules is used in media queries, where relative units
// are interpreted according to the document root element style, and styled only
// from the User Agent Stylesheet rules.
enum RuleMatchingBehavior {
    MatchAllRules,
    MatchAllRulesExcludingSMIL,
    MatchOnlyUserAgentRules,
};

const unsigned styleSharingListSize = 40;
typedef WTF::Deque<RefPtr<Element>, styleSharingListSize> StyleSharingList;

#undef STYLE_STATS

#ifdef STYLE_STATS
struct StyleSharingStats {
    void addSearch() { ++m_searches; ++m_totalSearches; }
    void addElementEligibleForSharing() { ++m_elementsEligibleForSharing; ++m_totalElementsEligibleForSharing; }
    void addStyleShared() { ++m_stylesShared; ++m_totalStylesShared; }
    void addSearchFoundSiblingForSharing() { ++m_searchFoundSiblingForSharing; ++m_totalSearchFoundSiblingForSharing; }
    void addSearchMissedSharing() { ++m_searchesMissedSharing; ++m_totalSearchesMissedSharing; }
    void addMatchedPropertiesSearch() { ++m_matchedPropertiesSearches; ++m_totalMatchedPropertiesSearches; }
    void addMatchedPropertiesHit() { ++m_matchedPropertiesHit; ++m_totalMatchedPropertiesHit; }
    void addMatchedPropertiesHitSharedInherited() { ++m_matchedPropertiesSharedInheritedHit; ++m_totalMatchedPropertiesSharedInheritedHit; }
    void addMatchedPropertiesToCache() { ++m_matchedPropertiesToCache; ++m_totalMatchedPropertiesToCache; }
    void addMatchedPropertiesEnteredIntoCache() { ++m_matchedPropertiesEnteredIntoCache; ++m_totalMatchedPropertiesEnteredIntoCache; }

    void clear()
    {
        m_searches = m_elementsEligibleForSharing = m_stylesShared = m_searchesMissedSharing = m_searchFoundSiblingForSharing =
            m_matchedPropertiesSearches = m_matchedPropertiesHit = m_matchedPropertiesSharedInheritedHit = m_matchedPropertiesToCache =
            m_matchedPropertiesEnteredIntoCache = 0;
    }

    void printStats() const;

    unsigned m_searches;
    unsigned m_elementsEligibleForSharing;
    unsigned m_stylesShared;
    unsigned m_searchFoundSiblingForSharing;
    unsigned m_searchesMissedSharing;
    unsigned m_matchedPropertiesSearches;
    unsigned m_matchedPropertiesHit;
    unsigned m_matchedPropertiesSharedInheritedHit;
    unsigned m_matchedPropertiesToCache;
    unsigned m_matchedPropertiesEnteredIntoCache;

    unsigned m_totalSearches;
    unsigned m_totalElementsEligibleForSharing;
    unsigned m_totalStylesShared;
    unsigned m_totalSearchFoundSiblingForSharing;
    unsigned m_totalSearchesMissedSharing;
    unsigned m_totalMatchedPropertiesSearches;
    unsigned m_totalMatchedPropertiesHit;
    unsigned m_totalMatchedPropertiesSharedInheritedHit;
    unsigned m_totalMatchedPropertiesToCache;
    unsigned m_totalMatchedPropertiesEnteredIntoCache;
};

#define STYLE_STATS_ADD_SEARCH() StyleResolver::styleSharingStats().addSearch();
#define STYLE_STATS_ADD_ELEMENT_ELIGIBLE_FOR_SHARING() StyleResolver::styleSharingStats().addElementEligibleForSharing();
#define STYLE_STATS_ADD_STYLE_SHARED() StyleResolver::styleSharingStats().addStyleShared();
#define STYLE_STATS_ADD_SEARCH_FOUND_SIBLING_FOR_SHARING() StyleResolver::styleSharingStats().addSearchFoundSiblingForSharing();
#define STYLE_STATS_ADD_SEARCH_MISSED_SHARING() StyleResolver::styleSharingStats().addSearchMissedSharing();
#define STYLE_STATS_PRINT() StyleResolver::styleSharingStats().printStats();
#define STYLE_STATS_CLEAR() StyleResolver::styleSharingStats().clear();
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_SEARCH() StyleResolver::styleSharingStats().addMatchedPropertiesSearch();
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_HIT() StyleResolver::styleSharingStats().addMatchedPropertiesHit();
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_HIT_SHARED_INHERITED() StyleResolver::styleSharingStats().addMatchedPropertiesHitSharedInherited();
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_TO_CACHE() StyleResolver::styleSharingStats().addMatchedPropertiesToCache();
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_ENTERED_INTO_CACHE() StyleResolver::styleSharingStats().addMatchedPropertiesEnteredIntoCache();
#else
#define STYLE_STATS_ADD_SEARCH() (void(0));
#define STYLE_STATS_ADD_ELEMENT_ELIGIBLE_FOR_SHARING() (void(0));
#define STYLE_STATS_ADD_STYLE_SHARED() (void(0));
#define STYLE_STATS_ADD_SEARCH_FOUND_SIBLING_FOR_SHARING() (void(0));
#define STYLE_STATS_ADD_SEARCH_MISSED_SHARING() (void(0));
#define STYLE_STATS_PRINT() (void(0));
#define STYLE_STATS_CLEAR() (void(0));
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_SEARCH() (void(0));
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_HIT() (void(0));
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_HIT_SHARED_INHERITED() (void(0));
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_TO_CACHE() (void(0));
#define STYLE_STATS_ADD_MATCHED_PROPERTIES_ENTERED_INTO_CACHE() (void(0));
#endif

struct CSSPropertyValue {
    CSSPropertyValue(CSSPropertyID property, CSSValue* value)
        : property(property), value(value) { }
    // Stores value=propertySet.getPropertyCSSValue(id).get().
    CSSPropertyValue(CSSPropertyID, const StylePropertySet&);
    CSSPropertyID property;
    CSSValue* value;
};

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class StyleResolver {
    WTF_MAKE_NONCOPYABLE(StyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    StyleResolver(Document&, bool matchAuthorAndUserStyles);
    ~StyleResolver();

    // FIXME: StyleResolver should not be keeping tree-walk state.
    // These should move to some global tree-walk state, or should be contained in a
    // TreeWalkContext or similar which is passed in to StyleResolver methods when available.
    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParentElement(Element*);
    void popParentElement(Element*);
    void pushParentShadowRoot(const ShadowRoot&);
    void popParentShadowRoot(const ShadowRoot&);

    PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules, RenderRegion* regionForStyling = 0);

    // FIXME: The following logic related to animations and keyframes should be factored out of StyleResolver
    // The body of calculateCSSAnimationUpdate can move to CSSAnimations.cpp and take just const element, const style,
    // and const ScopedStyleTree
    void calculateCSSAnimationUpdate(StyleResolverState&);
    void resolveKeyframes(const Element*, const RenderStyle*, const StringImpl* animationName, KeyframeAnimationEffect::KeyframeVector&);
    void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList&);
    const StyleRuleKeyframes* matchScopedKeyframesRule(const Element*, const StringImpl* animationName);
    PassRefPtr<RenderStyle> styleForKeyframe(Element*, const RenderStyle*, const StyleKeyframe*);

    PassRefPtr<RenderStyle> pseudoStyleForElement(Element*, const PseudoStyleRequest&, RenderStyle* parentStyle);

    PassRefPtr<RenderStyle> styleForPage(int pageIndex);
    PassRefPtr<RenderStyle> defaultStyleForElement();
    PassRefPtr<RenderStyle> styleForText(Text*);

    static PassRefPtr<RenderStyle> styleForDocument(Document&, CSSFontSelector* = 0);

    // FIXME: This only has 5 callers and should be removed. Callers should be explicit about
    // their dependency on Document* instead of grabbing one through StyleResolver.
    Document& document() { return m_document; }

    // FIXME: It could be better to call m_ruleSets.appendAuthorStyleSheets() directly after we factor StyleRsolver further.
    // https://bugs.webkit.org/show_bug.cgi?id=108890
    void appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >&);
    void resetAuthorStyle(const ContainerNode*);
    void resetAtHostRules(const ContainerNode*);
    void finishAppendAuthorStyleSheets();

    DocumentRuleSets& ruleSets() { return m_ruleSets; }
    const DocumentRuleSets& ruleSets() const { return m_ruleSets; }
    SelectorFilter& selectorFilter() { return m_selectorFilter; }

    void setBuildScopedStyleTreeInDocumentOrder(bool enabled) { m_styleTree.setBuildInDocumentOrder(enabled); }
    bool buildScopedStyleTreeInDocumentOrder() const { return m_styleTree.buildInDocumentOrder(); }

    ScopedStyleResolver* ensureScopedStyleResolver(const ContainerNode* scope)
    {
        return m_styleTree.ensureScopedStyleResolver(scope ? *scope : document());
    }

    // FIXME: Used by SharingStyleFinder, but should be removed.
    bool styleSharingCandidateMatchesRuleSet(const ElementResolveContext&, RenderStyle*, RuleSet*);

    // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
    enum CSSRuleFilter {
        UAAndUserCSSRules   = 1 << 1,
        AuthorCSSRules      = 1 << 2,
        EmptyCSSRules       = 1 << 3,
        CrossOriginCSSRules = 1 << 4,
        AllButEmptyCSSRules = UAAndUserCSSRules | AuthorCSSRules | CrossOriginCSSRules,
        AllCSSRules         = AllButEmptyCSSRules | EmptyCSSRules,
    };
    PassRefPtr<CSSRuleList> styleRulesForElement(Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    PassRefPtr<CSSRuleList> pseudoStyleRulesForElement(Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);

    // |properties| is an array with |count| elements.
    void applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle*);

    CSSFontSelector* fontSelector() const { return m_fontSelector.get(); }
    ViewportStyleResolver* viewportStyleResolver() { return m_viewportStyleResolver.get(); }

    // FIXME: This logic belongs in MediaQueryEvaluator.
    void addViewportDependentMediaQueryResult(const MediaQueryExp*, bool result);
    bool hasViewportDependentMediaQueries() const { return !m_viewportDependentMediaQueryResults.isEmpty(); }
    bool affectedByViewportChange() const;

    // FIXME: Regions should not require special logic in StyleResolver.
    bool checkRegionStyle(Element* regionElement);

    // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
    void invalidateMatchedPropertiesCache();

    // Exposed for RenderStyle::isStyleAvilable().
    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    // FIXME: StyleResolver should not have this member or method.
    InspectorCSSOMWrappers& inspectorCSSOMWrappers() { return m_inspectorCSSOMWrappers; }

    enum ViewportOrigin { UserAgentOrigin, AuthorOrigin };

    // Exposed for ScopedStyleResolver.
    // FIXME: Likely belongs on viewportStyleResolver.
    void collectViewportRules(RuleSet*, ViewportOrigin);

    const RuleFeatureSet& ruleFeatureSet() const { return m_features; }

    StyleSharingList& styleSharingList() { return m_styleSharingList; }

    void addToStyleSharingList(Element*);
    void clearStyleSharingList();

#ifdef STYLE_STATS
    ALWAYS_INLINE static StyleSharingStats& styleSharingStats() { return m_styleSharingStats; }
#endif
private:
    // FIXME: This should probably go away, folded into FontBuilder.
    void updateFont(StyleResolverState&);

    void matchUARules(ElementRuleCollector&, RuleSet*);
    void matchAuthorRules(Element*, ElementRuleCollector&, bool includeEmptyRules);
    void matchShadowDistributedRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchScopedAuthorRulesForShadowHost(Element*, ElementRuleCollector&, bool includeEmptyRules, Vector<ScopedStyleResolver*, 8>& resolvers, Vector<ScopedStyleResolver*, 8>& resolversInShadowTree);
    void matchHostRules(Element*, ScopedStyleResolver*, ElementRuleCollector&, bool includeEmptyRules);
    void matchScopedAuthorRules(Element*, ElementRuleCollector&, bool includeEmptyRules);
    void matchAllRules(StyleResolverState&, ElementRuleCollector&, bool matchAuthorAndUserStyles, bool includeSMILProperties);
    void matchUARules(ElementRuleCollector&);
    void matchUserRules(ElementRuleCollector&, bool includeEmptyRules);
    void collectFeatures();

    bool fastRejectSelector(const RuleData&) const;

    void applyMatchedProperties(StyleResolverState&, const MatchResult&);

    enum StyleApplicationPass {
        VariableDefinitions,
        AnimationProperties,
        HighPriorityProperties,
        LowPriorityProperties
    };
    template <StyleResolver::StyleApplicationPass pass>
    static inline bool isPropertyForPass(CSSPropertyID);
    template <StyleApplicationPass pass>
    void applyMatchedProperties(StyleResolverState&, const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly);
    template <StyleApplicationPass pass>
    void applyProperties(StyleResolverState&, const StylePropertySet* properties, StyleRule*, bool isImportant, bool inheritedOnly, PropertyWhitelistType = PropertyWhitelistNone);
    template <StyleApplicationPass pass>
    bool applyAnimatedProperties(StyleResolverState&, const DocumentTimeline*);
    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>&, bool isLeftPage, bool isFirstPage, const String& pageName);
    void collectViewportRules();
    Settings* documentSettings() { return m_document.settings(); }

    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    DocumentRuleSets m_ruleSets;

    // FIXME: This likely belongs on RuleSet.
    typedef HashMap<StringImpl*, RefPtr<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

    MatchedPropertiesCache m_matchedPropertiesCache;

    OwnPtr<MediaQueryEvaluator> m_medium;
    RefPtr<RenderStyle> m_rootDefaultStyle;

    Document& m_document;
    SelectorFilter m_selectorFilter;

    bool m_matchAuthorAndUserStyles;

    RefPtr<CSSFontSelector> m_fontSelector;
    Vector<OwnPtr<MediaQueryResult> > m_viewportDependentMediaQueryResults;

    RefPtr<ViewportStyleResolver> m_viewportStyleResolver;

    ScopedStyleTree m_styleTree;

    // FIXME: The entire logic of collecting features on StyleResolver, as well astransferring them
    // between various parts of machinery smells wrong. This needs to be better somehow.
    RuleFeatureSet m_features;
    OwnPtr<RuleSet> m_siblingRuleSet;
    OwnPtr<RuleSet> m_uncommonAttributeRuleSet;

    InspectorCSSOMWrappers m_inspectorCSSOMWrappers;

    StyleResourceLoader m_styleResourceLoader;

    StyleSharingList m_styleSharingList;

#ifdef STYLE_STATS
    static StyleSharingStats m_styleSharingStats;
#endif
};

inline bool checkRegionSelector(const CSSSelector* regionSelector, Element* regionElement)
{
    if (!regionSelector || !regionElement)
        return false;

    SelectorChecker selectorChecker(regionElement->document(), SelectorChecker::QueryingRules);
    for (const CSSSelector* s = regionSelector; s; s = CSSSelectorList::next(s)) {
        SelectorChecker::SelectorCheckingContext selectorCheckingContext(s, regionElement, SelectorChecker::VisitedMatchDisabled);
        PseudoId ignoreDynamicPseudo = NOPSEUDO;
        if (selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches)
            return true;
    }
    return false;
}

} // namespace WebCore

#endif // StyleResolver_h
