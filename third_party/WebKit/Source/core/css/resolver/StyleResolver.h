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

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSToStyleMap.h"
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
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/rendering/style/RenderStyle.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSCursorImageValue;
class CSSFontSelector;
class CSSImageGeneratorValue;
class CSSImageSetValue;
class CSSImageValue;
class CSSPageRule;
class CSSPrimitiveValue;
class CSSProperty;
class CSSRuleList;
class CSSSelector;
class CSSStyleSheet;
class CSSValue;
class ContainerNode;
class DeprecatedStyleBuilder;
class Document;
class Element;
class ElementRuleCollector;
class Frame;
class FrameView;
class KeyframeList;
class KeyframeValue;
class MediaQueryEvaluator;
class MediaQueryExp;
class MediaQueryResult;
class Node;
class RenderRegion;
class RuleData;
class RuleSet;
class Settings;
class StyleImage;
class StyleKeyframe;
class StylePendingImage;
class StylePropertySet;
class StyleRule;
class StyleRuleHost;
class StyleRuleKeyframes;
class StyleRulePage;
class StyleRuleRegion;
class StyleShader;
class StyleSheet;
class StyleSheetList;

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

// FIXME: Move to separate file.
class MatchRequest {
public:
    MatchRequest(RuleSet* ruleSet, bool includeEmptyRules = false, const ContainerNode* scope = 0)
        : ruleSet(ruleSet)
        , includeEmptyRules(includeEmptyRules)
        , scope(scope)
    {
        // Now that we're about to read from the RuleSet, we're done adding more
        // rules to the set and we should make sure it's compacted.
        ruleSet->compactRulesIfNeeded();
    }

    const RuleSet* ruleSet;
    const bool includeEmptyRules;
    const ContainerNode* scope;
};

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
    StyleResolver(Document*, bool matchAuthorAndUserStyles);
    ~StyleResolver();

    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParentElement(Element*);
    void popParentElement(Element*);
    void pushParentShadowRoot(const ShadowRoot*);
    void popParentShadowRoot(const ShadowRoot*);

    PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules, RenderRegion* regionForStyling = 0);

    void keyframeStylesForAnimation(Element*, const RenderStyle*, KeyframeList&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(Element*, const PseudoStyleRequest&, RenderStyle* parentStyle);

    PassRefPtr<RenderStyle> styleForPage(int pageIndex);
    PassRefPtr<RenderStyle> defaultStyleForElement();
    PassRefPtr<RenderStyle> styleForText(Text*);

    static PassRefPtr<RenderStyle> styleForDocument(Document*, CSSFontSelector* = 0);

    Document* document() { return m_document; }

    // FIXME: It could be better to call m_ruleSets.appendAuthorStyleSheets() directly after we factor StyleRsolver further.
    // https://bugs.webkit.org/show_bug.cgi?id=108890
    void appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >&);
    // FIXME: resetAuthorStyle() will be removed when rulesets are reset in a per-scoping node manner.
    void resetAuthorStyle();
    void resetAuthorStyle(const ContainerNode*);
    void resetAtHostRules(const ContainerNode*);

    DocumentRuleSets& ruleSets() { return m_ruleSets; }
    const DocumentRuleSets& ruleSets() const { return m_ruleSets; }
    SelectorFilter& selectorFilter() { return m_selectorFilter; }

    void setBuildScopedStyleTreeInDocumentOrder(bool enabled) { m_styleTree.setBuildInDocumentOrder(enabled); }
    bool buildScopedStyleTreeInDocumentOrder() const { return m_styleTree.buildInDocumentOrder(); }

    ScopedStyleResolver* ensureScopedStyleResolver(const ContainerNode* scope)
    {
        return m_styleTree.ensureScopedStyleResolver(scope ? scope : document());
    }

private:
    // FIXME: Move all of these into a separate style sharing controller object.
    RenderStyle* locateSharedStyle(const ElementResolveContext&);
    bool styleSharingCandidateMatchesRuleSet(RuleSet*);
    Node* locateCousinList(Element* parent, unsigned& visitedNodeCount) const;
    Element* findSiblingForStyleSharing(const ElementResolveContext&, Node*, unsigned& count) const;
    bool canShareStyleWithElement(const ElementResolveContext&, Element* sharingCandidate) const;
    bool canShareStyleWithControl(const ElementResolveContext&, Element* sharingCandidate) const;
    bool sharingCandidateHasIdenticalStyleAffectingAttributes(const ElementResolveContext&, Element* sharingCandidate) const;

    PassRefPtr<RenderStyle> styleForKeyframe(Element*, const RenderStyle*, const StyleKeyframe*, KeyframeValue&);

public:
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

public:
    // |properties| is an array with |count| elements.
    void applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle*);

    void updateFont();
    void initializeFontStyle(Settings*);
    void setFontSize(FontDescription&, float size);

public:
    bool hasSelectorForId(const AtomicString&) const;
    bool hasSelectorForClass(const AtomicString&) const;
    bool hasSelectorForAttribute(const AtomicString&) const;

    CSSFontSelector* fontSelector() const { return m_fontSelector.get(); }
    ViewportStyleResolver* viewportStyleResolver() { return m_viewportStyleResolver.get(); }

    void addViewportDependentMediaQueryResult(const MediaQueryExp*, bool result);
    bool hasViewportDependentMediaQueries() const { return !m_viewportDependentMediaQueryResults.isEmpty(); }
    bool affectedByViewportChange() const;

    void addKeyframeStyle(PassRefPtr<StyleRuleKeyframes>);

    bool checkRegionStyle(Element* regionElement);

    bool usesSiblingRules() const { return !m_features.siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_features.usesFirstLineRules; }
    bool usesBeforeAfterRules() const { return m_features.usesBeforeAfterRules; }

    // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
    void invalidateMatchedPropertiesCache();

private:
    void matchUARules(ElementRuleCollector&, RuleSet*);
    void matchAuthorRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchShadowDistributedRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchHostRules(ScopedStyleResolver*, ElementRuleCollector&, bool includeEmptyRules);
    void matchScopedAuthorRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchAllRules(ElementRuleCollector&, bool matchAuthorAndUserStyles, bool includeSMILProperties);
    void matchUARules(ElementRuleCollector&);
    void matchUserRules(ElementRuleCollector&, bool includeEmptyRules);
    void collectFeatures();

private:
    // This function fixes up the default font size if it detects that the current generic font family has changed. -dwh
    void checkForGenericFamilyChange(RenderStyle*, RenderStyle* parentStyle);
    void checkForZoomChange(RenderStyle*, RenderStyle* parentStyle);

    void adjustRenderStyle(RenderStyle* styleToAdjust, RenderStyle* parentStyle, Element*);
    void adjustGridItemPosition(RenderStyle* styleToAdjust) const;

    bool fastRejectSelector(const RuleData&) const;

    void applyMatchedProperties(const MatchResult&, const Element*);

    enum StyleApplicationPass {
        VariableDefinitions,
        AnimationProperties,
        HighPriorityProperties,
        LowPriorityProperties
    };
    template <StyleResolver::StyleApplicationPass pass>
    static inline bool isPropertyForPass(CSSPropertyID);
    template <StyleApplicationPass pass>
    void applyMatchedProperties(const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly);
    template <StyleApplicationPass pass>
    void applyProperties(const StylePropertySet* properties, StyleRule*, bool isImportant, bool inheritedOnly, PropertyWhitelistType = PropertyWhitelistNone);
    template <StyleApplicationPass pass>
    void applyAnimatedProperties(const Element* target);
    void resolveVariables(CSSPropertyID, CSSValue*, Vector<std::pair<CSSPropertyID, String> >& knownExpressions);
    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(Vector<StyleRulePage*>& matchedRules, const Vector<StyleRulePage*>&, bool isLeftPage, bool isFirstPage, const String& pageName);
    Settings* documentSettings() { return m_document->settings(); }

    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    DocumentRuleSets m_ruleSets;

    typedef HashMap<AtomicStringImpl*, RefPtr<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

public:
    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    InspectorCSSOMWrappers& inspectorCSSOMWrappers() { return m_inspectorCSSOMWrappers; }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

private:
    void applyProperty(CSSPropertyID, CSSValue*);

    void applySVGProperty(CSSPropertyID, CSSValue*);

    bool classNamesAffectedByRules(const SpaceSplitString&) const;

    MatchedPropertiesCache m_matchedPropertiesCache;

    OwnPtr<MediaQueryEvaluator> m_medium;
    RefPtr<RenderStyle> m_rootDefaultStyle;

    Document* m_document;
    SelectorFilter m_selectorFilter;

    bool m_matchAuthorAndUserStyles;

    RefPtr<CSSFontSelector> m_fontSelector;
    Vector<OwnPtr<MediaQueryResult> > m_viewportDependentMediaQueryResults;

    RefPtr<ViewportStyleResolver> m_viewportStyleResolver;

    const DeprecatedStyleBuilder& m_styleBuilder;
    ScopedStyleTree m_styleTree;

    RuleFeatureSet m_features;
    OwnPtr<RuleSet> m_siblingRuleSet;
    OwnPtr<RuleSet> m_uncommonAttributeRuleSet;

    InspectorCSSOMWrappers m_inspectorCSSOMWrappers;

    StyleResolverState m_state;
    StyleResourceLoader m_styleResourceLoader;

    bool m_elementAffectedByClassRules; // FIXME: Move this into a style sharing controller object.

    friend class DeprecatedStyleBuilder;
    friend void StyleBuilder::oldApplyProperty(CSSPropertyID, StyleResolver*, StyleResolverState&, CSSValue*, bool isInitial, bool isInherit);

};

inline bool StyleResolver::hasSelectorForAttribute(const AtomicString &attributeName) const
{
    ASSERT(!attributeName.isEmpty());
    return m_features.attrsInRules.contains(attributeName.impl());
}

inline bool StyleResolver::hasSelectorForClass(const AtomicString& classValue) const
{
    ASSERT(!classValue.isEmpty());
    return m_features.classesInRules.contains(classValue.impl());
}

inline bool StyleResolver::hasSelectorForId(const AtomicString& idValue) const
{
    ASSERT(!idValue.isEmpty());
    return m_features.idsInRules.contains(idValue.impl());
}

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
