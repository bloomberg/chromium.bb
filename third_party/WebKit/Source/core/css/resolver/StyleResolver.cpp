/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "core/css/resolver/StyleResolver.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "StylePropertyShorthand.h"
#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/Animation.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/FontFace.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"
#include "core/css/resolver/MatchResult.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "core/css/resolver/SharedStyleFinder.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFontFaceElement.h"
#include "wtf/StdLibExtras.h"

using namespace std;

namespace {

using namespace WebCore;

void setAnimationUpdateIfNeeded(StyleResolverState& state, Element& element)
{
    // If any changes to CSS Animations were detected, stash the update away for application after the
    // render object is updated if we're in the appropriate scope.
    if (state.animationUpdate())
        element.ensureActiveAnimations()->cssAnimations().setPendingUpdate(state.takeAnimationUpdate());
}

} // namespace

namespace WebCore {

using namespace HTMLNames;

RenderStyle* StyleResolver::s_styleNotYetAvailable;

static StylePropertySet* leftToRightDeclaration()
{
    DEFINE_STATIC_REF(MutableStylePropertySet, leftToRightDecl, (MutableStylePropertySet::create()));
    if (leftToRightDecl->isEmpty())
        leftToRightDecl->setProperty(CSSPropertyDirection, CSSValueLtr);
    return leftToRightDecl;
}

static StylePropertySet* rightToLeftDeclaration()
{
    DEFINE_STATIC_REF(MutableStylePropertySet, rightToLeftDecl, (MutableStylePropertySet::create()));
    if (rightToLeftDecl->isEmpty())
        rightToLeftDecl->setProperty(CSSPropertyDirection, CSSValueRtl);
    return rightToLeftDecl;
}

static void addFontFaceRule(Document* document, CSSFontSelector* cssFontSelector, const StyleRuleFontFace* fontFaceRule)
{
    RefPtr<FontFace> fontFace = FontFace::create(document, fontFaceRule);
    if (fontFace)
        cssFontSelector->fontFaceCache()->add(cssFontSelector, fontFaceRule, fontFace);
}

StyleResolver::StyleResolver(Document& document)
    : m_document(document)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
    , m_needCollectFeatures(false)
    , m_styleResourceLoader(document.fetcher())
    , m_styleResolverStatsSequence(0)
    , m_accessCount(0)
{
    // Construct document root element default style. This is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor because when constructor is run,
    // Document doesn't have documentElement.
    // NOTE: This assumes that element that gets passed to the styleForElement call
    // is always from the document that owns the StyleResolver.
    FrameView* view = document.view();
    if (view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType()));
    else
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));

    Element* root = document.documentElement();
    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, DisallowStyleSharing, MatchOnlyUserAgentRules);

    if (m_rootDefaultStyle && view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType(), &view->frame(), m_rootDefaultStyle.get()));

    m_styleTree.clear();

    initWatchedSelectorRules(CSSSelectorWatch::from(document).watchedCallbackSelectors());

#if ENABLE(SVG_FONTS)
    if (document.svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = document.svgExtensions()->svgFontFaceElements();
        HashSet<SVGFontFaceElement*>::const_iterator end = svgFontFaceElements.end();
        for (HashSet<SVGFontFaceElement*>::const_iterator it = svgFontFaceElements.begin(); it != end; ++it)
            addFontFaceRule(&document, document.styleEngine()->fontSelector(), (*it)->fontFaceRule());
    }
#endif
}

void StyleResolver::initWatchedSelectorRules(const Vector<RefPtr<StyleRule> >& watchedSelectors)
{
    if (!watchedSelectors.size())
        return;
    m_watchedSelectorsRules = RuleSet::create();
    for (unsigned i = 0; i < watchedSelectors.size(); ++i)
        m_watchedSelectorsRules->addStyleRule(watchedSelectors[i].get(), RuleHasNoSpecialState);
}

void StyleResolver::lazyAppendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >& styleSheets)
{
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i)
        m_pendingStyleSheets.add(styleSheets[i].get());
}

void StyleResolver::removePendingAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet> >& styleSheets)
{
    for (unsigned i = 0; i < styleSheets.size(); ++i)
        m_pendingStyleSheets.remove(styleSheets[i].get());
}

void StyleResolver::appendCSSStyleSheet(CSSStyleSheet* cssSheet)
{
    ASSERT(cssSheet);
    ASSERT(!cssSheet->disabled());
    if (cssSheet->mediaQueries() && !m_medium->eval(cssSheet->mediaQueries(), &m_viewportDependentMediaQueryResults))
        return;

    ContainerNode* scopingNode = ScopedStyleResolver::scopingNodeFor(document(), cssSheet);
    if (!scopingNode)
        return;

    ScopedStyleResolver* resolver = ensureScopedStyleResolver(scopingNode);
    ASSERT(resolver);
    resolver->addRulesFromSheet(cssSheet, *m_medium, this);
}

void StyleResolver::appendPendingAuthorStyleSheets()
{
    setBuildScopedStyleTreeInDocumentOrder(false);
    for (ListHashSet<CSSStyleSheet*, 16>::iterator it = m_pendingStyleSheets.begin(); it != m_pendingStyleSheets.end(); ++it)
        appendCSSStyleSheet(*it);

    m_pendingStyleSheets.clear();
    finishAppendAuthorStyleSheets();
}

void StyleResolver::appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >& styleSheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i)
        appendCSSStyleSheet(styleSheets[i].get());
}

void StyleResolver::finishAppendAuthorStyleSheets()
{
    collectFeatures();

    if (document().renderer() && document().renderer()->style())
        document().renderer()->style()->font().update(document().styleEngine()->fontSelector());

    collectViewportRules();

    document().styleEngine()->resetCSSFeatureFlags(m_features);
}

void StyleResolver::resetRuleFeatures()
{
    // Need to recreate RuleFeatureSet.
    m_features.clear();
    m_siblingRuleSet.clear();
    m_uncommonAttributeRuleSet.clear();
    m_needCollectFeatures = true;
}

void StyleResolver::addTreeBoundaryCrossingRules(const Vector<MinimalRuleData>& rules, ContainerNode* scope)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        const MinimalRuleData& info = rules[i];
        m_treeBoundaryCrossingRules.addRule(info.m_rule, info.m_selectorIndex, scope, info.m_flags);
    }
}

void StyleResolver::processScopedRules(const RuleSet& authorRules, const KURL& sheetBaseURL, ContainerNode* scope)
{
    const Vector<StyleRuleKeyframes*> keyframesRules = authorRules.keyframesRules();
    for (unsigned i = 0; i < keyframesRules.size(); ++i)
        ensureScopedStyleResolver(scope)->addKeyframeStyle(keyframesRules[i]);

    addTreeBoundaryCrossingRules(authorRules.treeBoundaryCrossingRules(), scope);

    // FIXME(BUG 72461): We don't add @font-face rules of scoped style sheets for the moment.
    if (!scope || scope->isDocumentNode()) {
        const Vector<StyleRuleFontFace*> fontFaceRules = authorRules.fontFaceRules();
        for (unsigned i = 0; i < fontFaceRules.size(); ++i)
            addFontFaceRule(&m_document, document().styleEngine()->fontSelector(), fontFaceRules[i]);
        if (fontFaceRules.size())
            invalidateMatchedPropertiesCache();
    } else {
        addTreeBoundaryCrossingRules(authorRules.shadowDistributedRules(), scope);
    }
}

void StyleResolver::resetAuthorStyle(const ContainerNode* scopingNode)
{
    // FIXME: When chanking scoped attribute, scopingNode's hasScopedHTMLStyleChild has been already modified.
    // So we cannot use hasScopedHTMLStyleChild flag here.
    ScopedStyleResolver* resolver = scopingNode ? m_styleTree.lookupScopedStyleResolverFor(scopingNode) : m_styleTree.scopedStyleResolverForDocument();
    if (!resolver)
        return;

    treeBoundaryCrossingRules().reset(scopingNode);

    resolver->resetAuthorStyle();
    resetRuleFeatures();
    if (!scopingNode)
        return;

    m_styleTree.remove(scopingNode);
}

static PassOwnPtr<RuleSet> makeRuleSet(const Vector<RuleFeature>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    OwnPtr<RuleSet> ruleSet = RuleSet::create();
    for (size_t i = 0; i < size; ++i)
        ruleSet->addRule(rules[i].rule, rules[i].selectorIndex, rules[i].hasDocumentSecurityOrigin ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState);
    return ruleSet.release();
}

void StyleResolver::collectFeatures()
{
    m_features.clear();
    // Collect all ids and rules using sibling selectors (:first-child and similar)
    // in the current set of stylesheets. Style sharing code uses this information to reject
    // sharing candidates.
    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    if (defaultStyleSheets.defaultStyle())
        m_features.add(defaultStyleSheets.defaultStyle()->features());

    if (document().isViewSource())
        m_features.add(defaultStyleSheets.defaultViewSourceStyle()->features());

    if (m_watchedSelectorsRules)
        m_features.add(m_watchedSelectorsRules->features());

    m_treeBoundaryCrossingRules.collectFeaturesTo(m_features);

    m_styleTree.collectFeaturesTo(m_features);

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);
    m_needCollectFeatures = false;
}

bool StyleResolver::hasRulesForId(const AtomicString& id) const
{
    return m_features.hasSelectorForId(id);
}

void StyleResolver::addToStyleSharingList(Element& element)
{
    // Never add elements to the style sharing list if we're not in a recalcStyle,
    // otherwise we could leave stale pointers in there.
    if (!document().inStyleRecalc())
        return;
    INCREMENT_STYLE_STATS_COUNTER(*this, sharedStyleCandidates);
    if (m_styleSharingList.size() >= styleSharingListSize)
        m_styleSharingList.remove(--m_styleSharingList.end());
    m_styleSharingList.prepend(&element);
}

void StyleResolver::clearStyleSharingList()
{
    m_styleSharingList.clear();
}

void StyleResolver::fontsNeedUpdate(CSSFontSelector* fontSelector)
{
    invalidateMatchedPropertiesCache();
    m_document.setNeedsStyleRecalc(SubtreeStyleChange);
}

void StyleResolver::pushParentElement(Element& parent)
{
    const ContainerNode* parentsParent = parent.parentOrShadowHostElement();

    // We are not always invoked consistently. For example, script execution can cause us to enter
    // style recalc in the middle of tree building. We may also be invoked from somewhere within the tree.
    // Reset the stack in this case, or if we see a new root element.
    // Otherwise just push the new parent.
    if (!parentsParent || m_selectorFilter.parentStackIsEmpty())
        m_selectorFilter.setupParentStack(parent);
    else
        m_selectorFilter.pushParent(parent);

    // Note: We mustn't skip ShadowRoot nodes for the scope stack.
    m_styleTree.pushStyleCache(parent, parent.parentOrShadowHostNode());
}

void StyleResolver::popParentElement(Element& parent)
{
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(&parent))
        m_selectorFilter.popParent();

    m_styleTree.popStyleCache(parent);
}

void StyleResolver::pushParentShadowRoot(const ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.host());
    m_styleTree.pushStyleCache(shadowRoot, shadowRoot.host());
}

void StyleResolver::popParentShadowRoot(const ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.host());
    m_styleTree.popStyleCache(shadowRoot);
}

StyleResolver::~StyleResolver()
{
    m_viewportStyleResolver->clearDocument();
}

inline void StyleResolver::collectTreeBoundaryCrossingRules(Element* element, ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_treeBoundaryCrossingRules.isEmpty())
        return;

    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();

    // When comparing rules declared in outer treescopes, outer's rules win.
    CascadeOrder outerCascadeOrder = m_treeBoundaryCrossingRules.size() + m_treeBoundaryCrossingRules.size();
    // When comparing rules declared in inner treescopes, inner's rules win.
    CascadeOrder innerCascadeOrder = m_treeBoundaryCrossingRules.size();

    for (DocumentOrderedList::iterator it = m_treeBoundaryCrossingRules.begin(); it != m_treeBoundaryCrossingRules.end(); ++it) {
        const ContainerNode* scopingNode = toContainerNode(*it);

        if (ShadowRoot* shadowRoot = scopingNode->containingShadowRoot()) {
            if (!shadowRoot->isActiveForStyling())
                continue;
        }

        RuleSet* ruleSet = m_treeBoundaryCrossingRules.ruleSetScopedBy(scopingNode);
        unsigned boundaryBehavior = SelectorChecker::ScopeContainsLastMatchedElement;
        bool isInnerTreeScope = element->treeScope().isInclusiveAncestorOf(scopingNode->treeScope());

        // If a given scoping node is a shadow root and a given element is in a descendant tree of tree hosted by
        // the scoping node's shadow host, we should use ScopeIsShadowHost.
        if (scopingNode && scopingNode->isShadowRoot()) {
            if (element->isInDescendantTreeOf(toShadowRoot(scopingNode)->host()))
                boundaryBehavior |= SelectorChecker::ScopeIsShadowHost;
            scopingNode = toShadowRoot(scopingNode)->host();
        }

        CascadeOrder cascadeOrder = isInnerTreeScope ? innerCascadeOrder : outerCascadeOrder;

        collector.collectMatchingRules(MatchRequest(ruleSet, includeEmptyRules, scopingNode), ruleRange, static_cast<SelectorChecker::BehaviorAtBoundary>(boundaryBehavior), ignoreCascadeScope, cascadeOrder);
        ++innerCascadeOrder;
        --outerCascadeOrder;
    }
}

static inline bool applyAuthorStylesOf(const Element* element)
{
    return element->treeScope().applyAuthorStyles() || (element->shadow() && element->shadow()->applyAuthorStyles());
}

void StyleResolver::matchAuthorRulesForShadowHost(Element* element, ElementRuleCollector& collector, bool includeEmptyRules, Vector<ScopedStyleResolver*, 8>& resolvers, Vector<ScopedStyleResolver*, 8>& resolversInShadowTree)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    CascadeScope cascadeScope = 0;
    CascadeOrder cascadeOrder = 0;
    bool applyAuthorStyles = applyAuthorStylesOf(element);

    for (int j = resolversInShadowTree.size() - 1; j >= 0; --j)
        resolversInShadowTree.at(j)->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, cascadeScope, cascadeOrder++);

    if (resolvers.isEmpty() || resolvers.first()->treeScope() != element->treeScope())
        ++cascadeScope;
    cascadeOrder += resolvers.size();
    for (unsigned i = 0; i < resolvers.size(); ++i)
        resolvers.at(i)->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, cascadeScope++, --cascadeOrder);

    collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchAuthorRules(Element* element, ElementRuleCollector& collector, bool includeEmptyRules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    bool applyAuthorStyles = applyAuthorStylesOf(element);
    if (m_styleTree.hasOnlyScopedResolverForDocument()) {
        m_styleTree.scopedStyleResolverForDocument()->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, ignoreCascadeScope);
        collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
        collector.sortAndTransferMatchedRules();
        return;
    }

    Vector<ScopedStyleResolver*, 8> resolvers;
    m_styleTree.resolveScopedStyles(element, resolvers);

    Vector<ScopedStyleResolver*, 8> resolversInShadowTree;
    m_styleTree.collectScopedResolversForHostedShadowTrees(element, resolversInShadowTree);
    if (!resolversInShadowTree.isEmpty()) {
        matchAuthorRulesForShadowHost(element, collector, includeEmptyRules, resolvers, resolversInShadowTree);
        return;
    }

    if (resolvers.isEmpty())
        return;

    CascadeScope cascadeScope = 0;
    CascadeOrder cascadeOrder = resolvers.size();
    for (unsigned i = 0; i < resolvers.size(); ++i, --cascadeOrder) {
        ScopedStyleResolver* resolver = resolvers.at(i);
        // FIXME: Need to clarify how to treat style scoped.
        resolver->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, cascadeScope++, resolver->treeScope() == element->treeScope() && resolver->scopingNode().isShadowRoot() ? 0 : cascadeOrder);
    }

    collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchWatchSelectorRules(ElementRuleCollector& collector)
{
    if (!m_watchedSelectorsRules)
        return;

    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUserRule = collector.matchedResult().matchedProperties.size() - 1;

    MatchRequest matchRequest(m_watchedSelectorsRules.get());
    RuleRange ruleRange = collector.matchedResult().ranges.userRuleRange();
    collector.collectMatchingRules(matchRequest, ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchUARules(ElementRuleCollector& collector)
{
    collector.setMatchingUARules(true);

    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    RuleSet* userAgentStyleSheet = m_medium->mediaTypeMatchSpecific("print")
        ? defaultStyleSheets.defaultPrintStyle() : defaultStyleSheets.defaultStyle();
    matchUARules(collector, userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (document().inQuirksMode())
        matchUARules(collector, defaultStyleSheets.defaultQuirksStyle());

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (document().isViewSource())
        matchUARules(collector, defaultStyleSheets.defaultViewSourceStyle());

    collector.setMatchingUARules(false);

    matchWatchSelectorRules(collector);
}

void StyleResolver::matchUARules(ElementRuleCollector& collector, RuleSet* rules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUARule = collector.matchedResult().matchedProperties.size() - 1;

    RuleRange ruleRange = collector.matchedResult().ranges.UARuleRange();
    collector.collectMatchingRules(MatchRequest(rules), ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchAllRules(StyleResolverState& state, ElementRuleCollector& collector, bool includeSMILProperties)
{
    matchUARules(collector);

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (state.element()->isStyledElement()) {
        collector.addElementStyleProperties(state.element()->presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        collector.addElementStyleProperties(state.element()->additionalPresentationAttributeStyle());

        if (state.element()->isHTMLElement()) {
            bool isAuto;
            TextDirection textDirection = toHTMLElement(state.element())->directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                collector.matchedResult().addMatchedProperties(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }

    matchAuthorRules(state.element(), collector, false);

    if (state.element()->isStyledElement()) {
        if (state.element()->inlineStyle()) {
            // Inline style is immutable as long as there is no CSSOM wrapper.
            bool isInlineStyleCacheable = !state.element()->inlineStyle()->isMutable();
            collector.addElementStyleProperties(state.element()->inlineStyle(), isInlineStyleCacheable);
        }

        // Now check SMIL animation override style.
        if (includeSMILProperties && state.element()->isSVGElement())
            collector.addElementStyleProperties(toSVGElement(state.element())->animatedSMILStyleProperties(), false /* isCacheable */);
    }
}

PassRefPtr<RenderStyle> StyleResolver::styleForDocument(Document& document, CSSFontSelector* fontSelector)
{
    const Frame* frame = document.frame();

    RefPtr<RenderStyle> documentStyle = RenderStyle::create();
    documentStyle->setDisplay(BLOCK);
    documentStyle->setRTLOrdering(document.visuallyOrdered() ? VisualOrder : LogicalOrder);
    documentStyle->setZoom(frame && !document.printing() ? frame->pageZoomFactor() : 1);
    documentStyle->setLocale(document.contentLanguage());

    // This overrides any -webkit-user-modify inherited from the parent iframe.
    documentStyle->setUserModify(document.inDesignMode() ? READ_WRITE : READ_ONLY);

    document.setupFontBuilder(documentStyle.get());
    return documentStyle.release();
}

// FIXME: This is duplicated with StyleAdjuster.cpp
// Perhaps this should move onto ElementResolveContext or even Element?
static inline bool isAtShadowBoundary(const Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

static inline void resetDirectionAndWritingModeOnDocument(Document& document)
{
    document.setDirectionSetOnDocumentElement(false);
    document.setWritingModeSetOnDocumentElement(false);
}

static void addContentAttrValuesToFeatures(const Vector<AtomicString>& contentAttrValues, RuleFeatureSet& features)
{
    for (size_t i = 0; i < contentAttrValues.size(); ++i)
        features.addAttributeInASelector(contentAttrValues[i]);
}

PassRefPtr<RenderStyle> StyleResolver::styleForElement(Element* element, RenderStyle* defaultParent, StyleSharingBehavior sharingBehavior,
    RuleMatchingBehavior matchingBehavior)
{
    ASSERT(document().frame());
    ASSERT(documentSettings());
    ASSERT(!hasPendingAuthorStyleSheets());
    ASSERT(!m_needCollectFeatures);

    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !element->document().haveStylesheetsLoaded() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(document().styleEngine()->fontSelector());
        }
        element->document().setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    didAccess();

    if (element == document().documentElement())
        resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), element, defaultParent);

    if (sharingBehavior == AllowStyleSharing && state.parentStyle()) {
        SharedStyleFinder styleFinder(state.elementContext(), m_features, m_siblingRuleSet.get(), m_uncommonAttributeRuleSet.get(), *this);
        if (RefPtr<RenderStyle> sharedStyle = styleFinder.findSharedStyle())
            return sharedStyle.release();
    }

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle(), isAtShadowBoundary(element) ? RenderStyle::AtShadowBoundary : RenderStyle::NotAtShadowBoundary);
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }
    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (state.distributedToInsertionPoint()) {
        if (Element* parent = element->parentElement()) {
            if (RenderStyle* styleOfShadowHost = parent->renderStyle())
                state.style()->setUserModify(styleOfShadowHost->userModify());
        }
    }

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    if (element->isLink()) {
        state.style()->setIsLink(true);
        EInsideLink linkState = state.elementLinkState();
        if (linkState != NotInsideLink) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoVisited);
            if (forceVisited)
                linkState = InsideVisitedLink;
        }
        state.style()->setInsideLink(linkState);
    }

    bool needsCollection = false;
    CSSDefaultStyleSheets::instance().ensureDefaultStyleSheetsForElement(element, needsCollection);
    if (needsCollection)
        collectFeatures();

    {
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());

        if (matchingBehavior == MatchOnlyUserAgentRules)
            matchUARules(collector);
        else
            matchAllRules(state, collector, matchingBehavior != MatchAllRulesExcludingSMIL);

        applyMatchedProperties(state, collector.matchedResult());

        addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);
    }
    {
        StyleAdjuster adjuster(state.cachedUAStyle(), m_document.inQuirksMode());
        adjuster.adjustRenderStyle(state.style(), state.parentStyle(), element);
    }

    // FIXME: The CSSWG wants to specify that the effects of animations are applied before
    // important rules, but this currently happens here as we require adjustment to have happened
    // before deciding which properties to transition.
    applyAnimatedProperties(state, element);

    // FIXME: Shouldn't this be on RenderBody::styleDidChange?
    if (element->hasTagName(bodyTag))
        document().textLinkColors().setTextColor(state.style()->color());

    setAnimationUpdateIfNeeded(state, *element);

    if (state.style()->hasViewportUnits())
        document().setHasViewportUnits();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForKeyframe(Element* element, const RenderStyle& elementStyle, RenderStyle* parentStyle, const StyleKeyframe* keyframe, const AtomicString& animationName)
{
    ASSERT(document().frame());
    ASSERT(documentSettings());
    ASSERT(!hasPendingAuthorStyleSheets());

    if (element == document().documentElement())
        resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), element, parentStyle);

    MatchResult result;
    if (keyframe->properties())
        result.addMatchedProperties(keyframe->properties());

    ASSERT(!state.style());

    // Create the style
    state.setStyle(RenderStyle::clone(&elementStyle));
    state.setLineHeightValue(0);

    // Make sure that the CSSAnimationData for the animation to which this
    // keyframe belongs is first in the list. This makes sure that if the
    // animation-timing-function property is set for this keyframe, it will be
    // applied to the correct CSSAnimationData object. Note that objects other
    // than the first in the list are ignored when reading the timing function
    // value. See KeyframeValue::timingFunction().
    CSSAnimationDataList* animations = state.style()->accessAnimations();
    ASSERT(animations && !animations->isEmpty());
    while (animations->animation(0)->name() != animationName)
        animations->remove(0);
    ASSERT(!animations->isEmpty() && animations->animation(0)->name() == animationName);

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    bool inheritedOnly = false;
    if (keyframe->properties()) {
        applyMatchedProperties<AnimationProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
        applyMatchedProperties<HighPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Line-height is set when we are sure we decided on the font-size
    if (state.lineHeightValue())
        StyleBuilder::applyProperty(CSSPropertyLineHeight, state, state.lineHeightValue());

    // Now do rest of the properties.
    if (keyframe->properties())
        applyMatchedProperties<LowPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied by one of the non-essential font props,
    // go ahead and update it a second time.
    updateFont(state);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());
    document().styleEngine()->fontSelector()->loadPendingFonts();

    didAccess();

    return state.takeStyle();
}

// This function is used by the WebAnimations JavaScript API method animate().
// FIXME: Remove this when animate() switches away from resolution-dependent parsing.
PassRefPtr<KeyframeEffectModel> StyleResolver::createKeyframeEffectModel(Element& element, const Vector<RefPtr<MutableStylePropertySet> >& propertySetVector, KeyframeEffectModel::KeyframeVector& keyframes)
{
    ASSERT(propertySetVector.size() == keyframes.size());

    StyleResolverState state(element.document(), &element);
    state.setStyle(RenderStyle::create());

    for (unsigned i = 0; i < propertySetVector.size(); ++i) {
        for (unsigned j = 0; j < propertySetVector[i]->propertyCount(); ++j) {
            CSSPropertyID id = propertySetVector[i]->propertyAt(j).id();
            StyleBuilder::applyProperty(id, state, propertySetVector[i]->getPropertyCSSValue(id).get());
            keyframes[i]->setPropertyValue(id, CSSAnimatableValueFactory::create(id, *state.style()).get());
        }
    }
    return KeyframeEffectModel::create(keyframes);
}

PassRefPtr<PseudoElement> StyleResolver::createPseudoElementIfNeeded(Element& parent, PseudoId pseudoId)
{
    RenderObject* parentRenderer = parent.renderer();
    if (!parentRenderer)
        return 0;

    if (pseudoId < FIRST_INTERNAL_PSEUDOID && !parentRenderer->style()->hasPseudoStyle(pseudoId))
        return 0;

    if (pseudoId == BACKDROP && !parent.isInTopLayer())
        return 0;

    if (!parentRenderer->canHaveGeneratedChildren())
        return 0;

    RenderStyle* parentStyle = parentRenderer->style();
    StyleResolverState state(document(), &parent, parentStyle);
    if (!pseudoStyleForElementInternal(parent, pseudoId, parentStyle, state))
        return 0;
    RefPtr<RenderStyle> style = state.takeStyle();
    ASSERT(style);

    if (!pseudoElementRendererIsNeeded(style.get()))
        return 0;

    parentStyle->addCachedPseudoStyle(style.release());
    RefPtr<PseudoElement> pseudo = PseudoElement::create(&parent, pseudoId);

    setAnimationUpdateIfNeeded(state, *pseudo);
    if (ActiveAnimations* activeAnimations = pseudo->activeAnimations())
        activeAnimations->cssAnimations().maybeApplyPendingUpdate(pseudo.get());
    return pseudo.release();
}

bool StyleResolver::pseudoStyleForElementInternal(Element& element, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle, StyleResolverState& state)
{
    ASSERT(document().frame());
    ASSERT(documentSettings());
    ASSERT(pseudoStyleRequest.pseudoId != FIRST_LINE_INHERITED);

    if (pseudoStyleRequest.allowsInheritance(state.parentStyle())) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    // Since we don't use pseudo-elements in any of our quirk/print
    // user agent rules, don't waste time walking those rules.

    {
        // Check UA, user and author rules.
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
        collector.setPseudoStyleRequest(pseudoStyleRequest);

        matchUARules(collector);
        matchAuthorRules(state.element(), collector, false);

        if (collector.matchedResult().matchedProperties.isEmpty())
            return false;

        state.style()->setStyleType(pseudoStyleRequest.pseudoId);

        applyMatchedProperties(state, collector.matchedResult());

        addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);
    }
    {
        StyleAdjuster adjuster(state.cachedUAStyle(), m_document.inQuirksMode());
        // FIXME: Passing 0 as the Element* introduces a lot of complexity
        // in the adjustRenderStyle code.
        adjuster.adjustRenderStyle(state.style(), state.parentStyle(), 0);
    }

    // FIXME: The CSSWG wants to specify that the effects of animations are applied before
    // important rules, but this currently happens here as we require adjustment to have happened
    // before deciding which properties to transition.
    applyAnimatedProperties(state, element.pseudoElement(pseudoStyleRequest.pseudoId));

    didAccess();

    if (state.style()->hasViewportUnits())
        document().setHasViewportUnits();

    return true;
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element* element, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle)
{
    ASSERT(parentStyle);
    if (!element)
        return 0;

    StyleResolverState state(document(), element, parentStyle);
    if (!pseudoStyleForElementInternal(*element, pseudoStyleRequest, parentStyle, state))
        return 0;

    if (PseudoElement* pseudoElement = element->pseudoElement(pseudoStyleRequest.pseudoId))
        setAnimationUpdateIfNeeded(state, *pseudoElement);

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    ASSERT(!hasPendingAuthorStyleSheets());
    resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), document().documentElement()); // m_rootElementStyle will be set to the document style.

    state.setStyle(RenderStyle::create());
    const RenderStyle* rootElementStyle = state.rootElementStyle() ? state.rootElementStyle() : document().renderStyle();
    ASSERT(rootElementStyle);
    state.style()->inheritFrom(rootElementStyle);

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    PageRuleCollector collector(rootElementStyle, pageIndex);

    collector.matchPageRules(CSSDefaultStyleSheets::instance().defaultPrintStyle());

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        scopedResolver->matchPageRules(collector);

    state.setLineHeightValue(0);
    bool inheritedOnly = false;

    MatchResult& result = collector.matchedResult();
    applyMatchedProperties<HighPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        StyleBuilder::applyProperty(CSSPropertyLineHeight, state, state.lineHeightValue());

    applyMatchedProperties<LowPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());
    document().styleEngine()->fontSelector()->loadPendingFonts();

    didAccess();

    // Now return the style.
    return state.takeStyle();
}

void StyleResolver::collectViewportRules()
{
    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    viewportStyleResolver()->collectViewportRules(defaultStyleSheets.defaultStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (!InspectorInstrumentation::applyViewportStyleOverride(&document(), this))
        viewportStyleResolver()->collectViewportRules(defaultStyleSheets.defaultViewportStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (document().isMobileDocument())
        viewportStyleResolver()->collectViewportRules(defaultStyleSheets.defaultXHTMLMobileProfileStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        scopedResolver->collectViewportRulesTo(this);

    viewportStyleResolver()->resolve();
}

PassRefPtr<RenderStyle> StyleResolver::defaultStyleForElement()
{
    StyleResolverState state(document(), 0);
    state.setStyle(RenderStyle::create());
    state.fontBuilder().initForStyleResolve(document(), state.style(), state.useSVGZoomRules());
    state.style()->setLineHeight(RenderStyle::initialLineHeight());
    state.setLineHeightValue(0);
    state.fontBuilder().setInitial(state.style()->effectiveZoom());
    state.style()->font().update(document().styleEngine()->fontSelector());
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForText(Text* textNode)
{
    ASSERT(textNode);

    NodeRenderingTraversal::ParentDetails parentDetails;
    Node* parentNode = NodeRenderingTraversal::parent(textNode, &parentDetails);
    if (!parentNode || !parentNode->renderStyle() || parentDetails.resetStyleInheritance())
        return defaultStyleForElement();
    return parentNode->renderStyle();
}

void StyleResolver::updateFont(StyleResolverState& state)
{
    state.fontBuilder().createFont(document().styleEngine()->fontSelector(), state.parentStyle(), state.style());
    if (state.fontBuilder().fontSizeHasViewportUnits())
        state.style()->setHasViewportUnits();
}

PassRefPtr<StyleRuleList> StyleResolver::styleRulesForElement(Element* element, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingStyleRules);
    collectPseudoRulesForElement(element, collector, NOPSEUDO, rulesToInclude);
    return collector.matchedStyleRuleList();
}

PassRefPtr<CSSRuleList> StyleResolver::pseudoCSSRulesForElement(Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingCSSRules);
    collectPseudoRulesForElement(element, collector, pseudoId, rulesToInclude);
    return collector.matchedCSSRuleList();
}

PassRefPtr<CSSRuleList> StyleResolver::cssRulesForElement(Element* element, unsigned rulesToInclude)
{
    return pseudoCSSRulesForElement(element, NOPSEUDO, rulesToInclude);
}

void StyleResolver::collectPseudoRulesForElement(Element* element, ElementRuleCollector& collector, PseudoId pseudoId, unsigned rulesToInclude)
{
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));

    if (rulesToInclude & UAAndUserCSSRules)
        matchUARules(collector);

    if (rulesToInclude & AuthorCSSRules) {
        collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));
        matchAuthorRules(element, collector, rulesToInclude & EmptyCSSRules);
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

void StyleResolver::applyAnimatedProperties(StyleResolverState& state, Element* animatingElement)
{
    const Element* element = state.element();
    ASSERT(element);

    // The animating element may be this element, or its pseudo element. It is
    // null when calculating the style for a potential pseudo element that has
    // yet to be created.
    ASSERT(animatingElement == element || !animatingElement || animatingElement->parentOrShadowHostElement() == element);

    if (!(animatingElement && animatingElement->hasActiveAnimations())
        && !(state.style()->transitions() && !state.style()->transitions()->isEmpty())
        && !(state.style()->animations() && !state.style()->animations()->isEmpty()))
        return;

    state.setAnimationUpdate(CSSAnimations::calculateUpdate(animatingElement, *element, *state.style(), state.parentStyle(), this));
    if (!state.animationUpdate())
        return;

    const AnimationEffect::CompositableValueMap& compositableValuesForAnimations = state.animationUpdate()->compositableValuesForAnimations();
    const AnimationEffect::CompositableValueMap& compositableValuesForTransitions = state.animationUpdate()->compositableValuesForTransitions();
    applyAnimatedProperties<HighPriorityProperties>(state, compositableValuesForAnimations);
    applyAnimatedProperties<HighPriorityProperties>(state, compositableValuesForTransitions);
    applyAnimatedProperties<LowPriorityProperties>(state, compositableValuesForAnimations);
    applyAnimatedProperties<LowPriorityProperties>(state, compositableValuesForTransitions);

    // If the animations/transitions change opacity or transform, we need to update
    // the style to impose the stacking rules. Note that this is also
    // done in StyleResolver::adjustRenderStyle().
    RenderStyle* style = state.style();
    if (style->hasAutoZIndex() && (style->opacity() < 1.0f || style->hasTransform()))
        style->setZIndex(0);
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyAnimatedProperties(StyleResolverState& state, const AnimationEffect::CompositableValueMap& compositableValues)
{
    ASSERT(pass != AnimationProperties);

    for (AnimationEffect::CompositableValueMap::const_iterator iter = compositableValues.begin(); iter != compositableValues.end(); ++iter) {
        CSSPropertyID property = iter->key;
        if (!isPropertyForPass<pass>(property))
            continue;
        ASSERT_WITH_MESSAGE(!iter->value->dependsOnUnderlyingValue(), "Web Animations not yet implemented: An interface for compositing onto the underlying value.");
        RefPtr<AnimatableValue> animatableValue = iter->value->compositeOnto(0);
        AnimatedStyleBuilder::applyProperty(property, state, animatableValue.get());
    }
}

static inline bool isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    // FIXME: 'text-decoration' shorthand to be handled when available.
    // See https://chromiumcodereview.appspot.com/19516002 for details.
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
        return RuntimeEnabledFeatures::css3TextDecorationsEnabled();
    default:
        break;
    }
    return false;
}

template <StyleResolver::StyleApplicationPass pass>
bool StyleResolver::isPropertyForPass(CSSPropertyID property)
{
    const CSSPropertyID firstAnimationProperty = CSSPropertyDisplay;
    const CSSPropertyID lastAnimationProperty = CSSPropertyTransitionTimingFunction;
    COMPILE_ASSERT(firstCSSProperty == firstAnimationProperty, CSS_first_animation_property_should_be_first_property);
    const CSSPropertyID firstHighPriorityProperty = CSSPropertyColor;
    const CSSPropertyID lastHighPriorityProperty = CSSPropertyLineHeight;
    COMPILE_ASSERT(lastAnimationProperty + 1 == firstHighPriorityProperty, CSS_color_is_first_high_priority_property);
    COMPILE_ASSERT(CSSPropertyLineHeight == firstHighPriorityProperty + 17, CSS_line_height_is_end_of_high_prioity_property_range);
    COMPILE_ASSERT(CSSPropertyZoom == lastHighPriorityProperty - 1, CSS_zoom_is_before_line_height);
    switch (pass) {
    case AnimationProperties:
        return property >= firstAnimationProperty && property <= lastAnimationProperty;
    case HighPriorityProperties:
        return property >= firstHighPriorityProperty && property <= lastHighPriorityProperty;
    case LowPriorityProperties:
        return property > lastHighPriorityProperty;
    }
    ASSERT_NOT_REACHED();
    return false;
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyProperties(StyleResolverState& state, const StylePropertySet* properties, StyleRule* rule, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType)
{
    state.setCurrentRule(rule);

    unsigned propertyCount = properties->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        StylePropertySet::PropertyReference current = properties->propertyAt(i);
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }
        CSSPropertyID property = current.id();

        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(property))
            continue;
        if (!isPropertyForPass<pass>(property))
            continue;
        if (pass == HighPriorityProperties && property == CSSPropertyLineHeight)
            state.setLineHeightValue(current.value());
        else
            StyleBuilder::applyProperty(current.id(), state, current.value());
    }
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult, bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    if (state.style()->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
            unsigned linkMatchType = matchedProperties.linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            state.setApplyPropertyToRegularStyle(linkMatchType & SelectorChecker::MatchLink);
            state.setApplyPropertyToVisitedLinkStyle(linkMatchType & SelectorChecker::MatchVisited);

            applyProperties<pass>(state, matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
        }
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        applyProperties<pass>(state, matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
    }
}

static unsigned computeMatchedPropertiesHash(const MatchedProperties* properties, unsigned size)
{
    return StringHasher::hashMemory(properties, sizeof(MatchedProperties) * size);
}

void StyleResolver::invalidateMatchedPropertiesCache()
{
    m_matchedPropertiesCache.clear();
}

void StyleResolver::notifyResizeForViewportUnits()
{
    collectViewportRules();
    m_matchedPropertiesCache.clearViewportDependent();
}

void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult)
{
    const Element* element = state.element();
    ASSERT(element);

    INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyApply);

    unsigned cacheHash = matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const CachedMatchedProperties* cachedMatchedProperties = 0;

    if (cacheHash && (cachedMatchedProperties = m_matchedPropertiesCache.find(cacheHash, state, matchResult))
        && MatchedPropertiesCache::isCacheable(element, state.style(), state.parentStyle())) {
        INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheHit);
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cachedMatchedProperties->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cachedMatchedProperties->parentRenderStyle.get()) && !isAtShadowBoundary(element)
            && (!state.distributedToInsertionPoint() || state.style()->userModify() == READ_ONLY)) {
            INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheInheritedHit);

            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(cachedMatchedProperties->renderStyle.get());

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true;
    }

    // Apply animation properties in order to apply animation results and trigger transitions below.
    applyMatchedProperties<AnimationProperties>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Match transition-property / animation-name length by trimming and
    // lengthening other transition / animation property lists
    // FIXME: This is wrong because we shouldn't affect the computed values
    state.style()->adjustAnimations();
    state.style()->adjustTransitions();

    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    state.setLineHeightValue(0);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->effectiveZoom() != state.style()->effectiveZoom()) {
        state.fontBuilder().setFontDirty(true);
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        StyleBuilder::applyProperty(CSSPropertyLineHeight, state, state.lineHeightValue());

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->fontDescription() != state.style()->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Cache the UA properties to pass them to RenderTheme in adjustRenderStyle.
    state.cacheUserAgentBorderAndBackground();

    // Now do the author and user normal priority properties and all the !important properties.
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, false, matchResult.ranges.lastUARule + 1, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());
    document().styleEngine()->fontSelector()->loadPendingFonts();

    if (!cachedMatchedProperties && cacheHash && MatchedPropertiesCache::isCacheable(element, state.style(), state.parentStyle())) {
        INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheAdded);
        m_matchedPropertiesCache.add(state.style(), state.parentStyle(), cacheHash, matchResult);
    }

    ASSERT(!state.fontBuilder().fontDirty());
}

CSSPropertyValue::CSSPropertyValue(CSSPropertyID id, const StylePropertySet& propertySet)
    : property(id), value(propertySet.getPropertyCSSValue(id).get())
{ }

void StyleResolver::enableStats(StatsReportType reportType)
{
    if (m_styleResolverStats)
        return;
    m_styleResolverStats = StyleResolverStats::create();
    m_styleResolverStatsTotals = StyleResolverStats::create();
    if (reportType == ReportSlowStats) {
        m_styleResolverStats->printMissedCandidateCount = true;
        m_styleResolverStatsTotals->printMissedCandidateCount = true;
    }
}

void StyleResolver::disableStats()
{
    m_styleResolverStatsSequence = 0;
    m_styleResolverStats.clear();
    m_styleResolverStatsTotals.clear();
}

void StyleResolver::printStats()
{
    if (!m_styleResolverStats)
        return;
    fprintf(stderr, "=== Style Resolver Stats (resolve #%u) (%s) ===\n", ++m_styleResolverStatsSequence, m_document.url().string().utf8().data());
    fprintf(stderr, "%s\n", m_styleResolverStats->report().utf8().data());
    fprintf(stderr, "== Totals ==\n");
    fprintf(stderr, "%s\n", m_styleResolverStatsTotals->report().utf8().data());
}

void StyleResolver::applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle* style)
{
    StyleResolverState state(document(), document().documentElement(), style);
    state.setStyle(style);

    state.fontBuilder().initForStyleResolve(document(), style, state.useSVGZoomRules());

    for (size_t i = 0; i < count; ++i) {
        if (properties[i].value) {
            // As described in BUG66291, setting font-size and line-height on a font may entail a CSSPrimitiveValue::computeLengthDouble call,
            // which assumes the fontMetrics are available for the affected font, otherwise a crash occurs (see http://trac.webkit.org/changeset/96122).
            // The updateFont() call below updates the fontMetrics and ensure the proper setting of font-size and line-height.
            switch (properties[i].property) {
            case CSSPropertyFontSize:
            case CSSPropertyLineHeight:
                updateFont(state);
                break;
            default:
                break;
            }
            StyleBuilder::applyProperty(properties[i].property, state, properties[i].value);
        }
    }
}

void StyleResolver::addMediaQueryResults(const MediaQueryResultList& list)
{
    for (size_t i = 0; i < list.size(); ++i)
        m_viewportDependentMediaQueryResults.append(list[i]);
}

bool StyleResolver::mediaQueryAffectedByViewportChange() const
{
    for (unsigned i = 0; i < m_viewportDependentMediaQueryResults.size(); ++i) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

} // namespace WebCore
