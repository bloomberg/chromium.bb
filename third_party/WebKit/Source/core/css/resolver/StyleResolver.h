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

#include "core/animation/KeyframeEffectModel.h"
#include "core/css/CSSFontSelectorClient.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorFilter.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/css/TreeBoundaryCrossingRules.h"
#include "core/css/resolver/MatchedPropertiesCache.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/ScopedStyleTree.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/StyleResourceLoader.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
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
class MediaQueryResult;
class RuleData;
class Settings;
class StyleKeyframe;
class StylePropertySet;
class StyleResolverStats;
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
typedef WTF::Deque<Element*, styleSharingListSize> StyleSharingList;

struct CSSPropertyValue {
    CSSPropertyValue(CSSPropertyID property, CSSValue* value)
        : property(property), value(value) { }
    // Stores value=propertySet.getPropertyCSSValue(id).get().
    CSSPropertyValue(CSSPropertyID, const StylePropertySet&);
    CSSPropertyID property;
    CSSValue* value;
};

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class StyleResolver FINAL : public CSSFontSelectorClient {
    WTF_MAKE_NONCOPYABLE(StyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit StyleResolver(Document&);
    virtual ~StyleResolver();

    // FIXME: StyleResolver should not be keeping tree-walk state.
    // These should move to some global tree-walk state, or should be contained in a
    // TreeWalkContext or similar which is passed in to StyleResolver methods when available.
    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParentElement(Element&);
    void popParentElement(Element&);
    void pushParentShadowRoot(const ShadowRoot&);
    void popParentShadowRoot(const ShadowRoot&);

    PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules);

    PassRefPtr<RenderStyle> styleForKeyframe(Element*, const RenderStyle&, RenderStyle* parentStyle, const StyleKeyframe*, const AtomicString& animationName);
    static PassRefPtrWillBeRawPtr<KeyframeEffectModel> createKeyframeEffectModel(Element&, const Vector<RefPtr<MutableStylePropertySet> >&, KeyframeEffectModel::KeyframeVector&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(Element*, const PseudoStyleRequest&, RenderStyle* parentStyle);

    PassRefPtr<RenderStyle> styleForPage(int pageIndex);
    PassRefPtr<RenderStyle> defaultStyleForElement();
    PassRefPtr<RenderStyle> styleForText(Text*);

    static PassRefPtr<RenderStyle> styleForDocument(Document&, CSSFontSelector* = 0);

    // FIXME: This only has 5 callers and should be removed. Callers should be explicit about
    // their dependency on Document* instead of grabbing one through StyleResolver.
    Document& document() { return m_document; }

    // FIXME: It could be better to call appendAuthorStyleSheets() directly after we factor StyleResolver further.
    // https://bugs.webkit.org/show_bug.cgi?id=108890
    void appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >&);
    void resetAuthorStyle(const ContainerNode*);
    void finishAppendAuthorStyleSheets();

    TreeBoundaryCrossingRules& treeBoundaryCrossingRules() { return m_treeBoundaryCrossingRules; }
    void processScopedRules(const RuleSet& authorRules, const KURL&, ContainerNode* scope = 0);

    void lazyAppendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >&);
    void removePendingAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet> >&);
    void appendPendingAuthorStyleSheets();
    bool hasPendingAuthorStyleSheets() const { return m_pendingStyleSheets.size() > 0 || m_needCollectFeatures; }

    SelectorFilter& selectorFilter() { return m_selectorFilter; }

    void setBuildScopedStyleTreeInDocumentOrder(bool enabled) { m_styleTree.setBuildInDocumentOrder(enabled); }
    bool buildScopedStyleTreeInDocumentOrder() const { return m_styleTree.buildInDocumentOrder(); }
    bool styleTreeHasOnlyScopedResolverForDocument() const { return m_styleTree.hasOnlyScopedResolverForDocument(); }
    ScopedStyleResolver* styleTreeScopedStyleResolverForDocument() const { return m_styleTree.scopedStyleResolverForDocument(); }

    ScopedStyleResolver* ensureScopedStyleResolver(ContainerNode* scope)
    {
        ASSERT(scope);
        return m_styleTree.ensureScopedStyleResolver(*scope);
    }

    void styleTreeResolveScopedKeyframesRules(const Element* element, Vector<ScopedStyleResolver*, 8>& resolvers)
    {
        m_styleTree.resolveScopedKeyframesRules(element, resolvers);
    }

    // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
    enum CSSRuleFilter {
        UAAndUserCSSRules   = 1 << 1,
        AuthorCSSRules      = 1 << 2,
        EmptyCSSRules       = 1 << 3,
        CrossOriginCSSRules = 1 << 4,
        AllButEmptyCSSRules = UAAndUserCSSRules | AuthorCSSRules | CrossOriginCSSRules,
        AllCSSRules         = AllButEmptyCSSRules | EmptyCSSRules,
    };
    PassRefPtr<CSSRuleList> cssRulesForElement(Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    PassRefPtr<CSSRuleList> pseudoCSSRulesForElement(Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);
    PassRefPtr<StyleRuleList> styleRulesForElement(Element*, unsigned rulesToInclude);

    // |properties| is an array with |count| elements.
    void applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle*);

    ViewportStyleResolver* viewportStyleResolver() { return m_viewportStyleResolver.get(); }

    void addMediaQueryResults(const MediaQueryResultList&);
    MediaQueryResultList* viewportDependentMediaQueryResults() { return &m_viewportDependentMediaQueryResults; }
    bool hasViewportDependentMediaQueries() const { return !m_viewportDependentMediaQueryResults.isEmpty(); }
    bool mediaQueryAffectedByViewportChange() const;

    // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
    void invalidateMatchedPropertiesCache();

    void notifyResizeForViewportUnits();

    // Exposed for RenderStyle::isStyleAvilable().
    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    RuleFeatureSet& ensureRuleFeatureSet()
    {
        if (hasPendingAuthorStyleSheets())
            appendPendingAuthorStyleSheets();
        return m_features;
    }

    StyleSharingList& styleSharingList() { return m_styleSharingList; }

    bool hasRulesForId(const AtomicString&) const;

    void addToStyleSharingList(Element&);
    void clearStyleSharingList();

    StyleResolverStats* stats() { return m_styleResolverStats.get(); }
    StyleResolverStats* statsTotals() { return m_styleResolverStatsTotals.get(); }
    enum StatsReportType { ReportDefaultStats, ReportSlowStats };
    void enableStats(StatsReportType = ReportDefaultStats);
    void disableStats();
    void printStats();

    unsigned accessCount() const { return m_accessCount; }
    void didAccess() { ++m_accessCount; }

    PassRefPtr<PseudoElement> createPseudoElementIfNeeded(Element& parent, PseudoId);

private:
    // CSSFontSelectorClient implementation.
    virtual void fontsNeedUpdate(CSSFontSelector*) OVERRIDE;

private:
    void initWatchedSelectorRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRule> >& watchedSelectors);

    void addTreeBoundaryCrossingRules(const Vector<MinimalRuleData>&, ContainerNode* scope);

    // FIXME: This should probably go away, folded into FontBuilder.
    void updateFont(StyleResolverState&);

    void appendCSSStyleSheet(CSSStyleSheet*);

    void collectPseudoRulesForElement(Element*, ElementRuleCollector&, PseudoId, unsigned rulesToInclude);
    void matchUARules(ElementRuleCollector&, RuleSet*);
    void matchAuthorRules(Element*, ElementRuleCollector&, bool includeEmptyRules);
    void matchAuthorRulesForShadowHost(Element*, ElementRuleCollector&, bool includeEmptyRules, Vector<ScopedStyleResolver*, 8>& resolvers, Vector<ScopedStyleResolver*, 8>& resolversInShadowTree);
    void matchAllRules(StyleResolverState&, ElementRuleCollector&, bool includeSMILProperties);
    void matchUARules(ElementRuleCollector&);
    // FIXME: watched selectors should be implemented using injected author stylesheets: http://crbug.com/316960
    void matchWatchSelectorRules(ElementRuleCollector&);
    void collectFeatures();
    void collectTreeBoundaryCrossingRules(Element*, ElementRuleCollector&, bool includeEmptyRules);
    void resetRuleFeatures();

    bool fastRejectSelector(const RuleData&) const;

    void applyMatchedProperties(StyleResolverState&, const MatchResult&);
    void applyAnimatedProperties(StyleResolverState&, Element* animatingElement);

    enum StyleApplicationPass {
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
    void applyAnimatedProperties(StyleResolverState&, const AnimationEffect::CompositableValueMap&);
    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>&, bool isLeftPage, bool isFirstPage, const String& pageName);
    void collectViewportRules();
    Settings* documentSettings() { return m_document.settings(); }

    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    bool pseudoStyleForElementInternal(Element&, const PseudoStyleRequest&, RenderStyle* parentStyle, StyleResolverState&);

    // FIXME: This likely belongs on RuleSet.
    typedef WillBePersistentHeapHashMap<StringImpl*, RefPtrWillBeMember<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

    MatchedPropertiesCache m_matchedPropertiesCache;

    OwnPtr<MediaQueryEvaluator> m_medium;
    MediaQueryResultList m_viewportDependentMediaQueryResults;

    RefPtr<RenderStyle> m_rootDefaultStyle;

    Document& m_document;
    SelectorFilter m_selectorFilter;

    RefPtr<ViewportStyleResolver> m_viewportStyleResolver;

    ListHashSet<CSSStyleSheet*, 16> m_pendingStyleSheets;

    ScopedStyleTree m_styleTree;

    // FIXME: The entire logic of collecting features on StyleResolver, as well as transferring them
    // between various parts of machinery smells wrong. This needs to be better somehow.
    RuleFeatureSet m_features;
    OwnPtr<RuleSet> m_siblingRuleSet;
    OwnPtr<RuleSet> m_uncommonAttributeRuleSet;

    // FIXME: watched selectors should be implemented using injected author stylesheets: http://crbug.com/316960
    OwnPtr<RuleSet> m_watchedSelectorsRules;
    TreeBoundaryCrossingRules m_treeBoundaryCrossingRules;

    bool m_needCollectFeatures;

    StyleResourceLoader m_styleResourceLoader;

    StyleSharingList m_styleSharingList;

    OwnPtr<StyleResolverStats> m_styleResolverStats;
    OwnPtr<StyleResolverStats> m_styleResolverStatsTotals;
    unsigned m_styleResolverStatsSequence;

    // Use only for Internals::updateStyleAndReturnAffectedElementCount.
    unsigned m_accessCount;
};

} // namespace WebCore

#endif // StyleResolver_h
