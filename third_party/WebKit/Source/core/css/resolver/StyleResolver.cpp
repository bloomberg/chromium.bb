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

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/StyleInterpolation.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePool.h"
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
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/SharedStyleFinder.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/css/resolver/StyleResolverParentScope.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/NodeLayoutStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutView.h"
#include "core/layout/style/KeyframeList.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/StdLibExtras.h"

namespace {

using namespace blink;

void setAnimationUpdateIfNeeded(StyleResolverState& state, Element& element)
{
    // If any changes to CSS Animations were detected, stash the update away for application after the
    // render object is updated if we're in the appropriate scope.
    if (state.animationUpdate())
        element.ensureElementAnimations().cssAnimations().setPendingUpdate(state.takeAnimationUpdate());
}

} // namespace

namespace blink {

using namespace HTMLNames;

LayoutStyle* StyleResolver::s_styleNotYetAvailable;

static StylePropertySet* leftToRightDeclaration()
{
    DEFINE_STATIC_REF_WILL_BE_PERSISTENT(MutableStylePropertySet, leftToRightDecl, (MutableStylePropertySet::create()));
    if (leftToRightDecl->isEmpty())
        leftToRightDecl->setProperty(CSSPropertyDirection, CSSValueLtr);
    return leftToRightDecl;
}

static StylePropertySet* rightToLeftDeclaration()
{
    DEFINE_STATIC_REF_WILL_BE_PERSISTENT(MutableStylePropertySet, rightToLeftDecl, (MutableStylePropertySet::create()));
    if (rightToLeftDecl->isEmpty())
        rightToLeftDecl->setProperty(CSSPropertyDirection, CSSValueRtl);
    return rightToLeftDecl;
}

static void collectScopedResolversForHostedShadowTrees(const Element* element, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolvers)
{
    ElementShadow* shadow = element->shadow();
    if (!shadow)
        return;

    // Adding scoped resolver for active shadow roots for shadow host styling.
    for (ShadowRoot* shadowRoot = shadow->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
        if (shadowRoot->numberOfStyles() > 0) {
            if (ScopedStyleResolver* resolver = shadowRoot->scopedStyleResolver())
                resolvers.append(resolver);
        }
    }
}

StyleResolver::StyleResolver(Document& document)
    : m_document(document)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
    , m_needCollectFeatures(false)
    , m_printMediaType(false)
    , m_styleResourceLoader(&document)
    , m_styleSharingDepth(0)
    , m_styleResolverStatsSequence(0)
    , m_accessCount(0)
{
    FrameView* view = document.view();
    if (view) {
        m_medium = adoptPtr(new MediaQueryEvaluator(&view->frame()));
        m_printMediaType = equalIgnoringCase(view->mediaType(), MediaTypeNames::print);
    } else {
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));
    }

    initWatchedSelectorRules(CSSSelectorWatch::from(document).watchedCallbackSelectors());
}

void StyleResolver::initWatchedSelectorRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRule>>& watchedSelectors)
{
    if (!watchedSelectors.size())
        return;
    m_watchedSelectorsRules = RuleSet::create();
    for (unsigned i = 0; i < watchedSelectors.size(); ++i)
        m_watchedSelectorsRules->addStyleRule(watchedSelectors[i].get(), RuleHasNoSpecialState);
}

void StyleResolver::lazyAppendAuthorStyleSheets(unsigned firstNew, const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet>>& styleSheets)
{
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i)
        m_pendingStyleSheets.add(styleSheets[i].get());
}

void StyleResolver::removePendingAuthorStyleSheets(const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet>>& styleSheets)
{
    for (unsigned i = 0; i < styleSheets.size(); ++i)
        m_pendingStyleSheets.remove(styleSheets[i].get());
}

void StyleResolver::appendCSSStyleSheet(CSSStyleSheet& cssSheet)
{
    ASSERT(!cssSheet.disabled());
    if (cssSheet.mediaQueries() && !m_medium->eval(cssSheet.mediaQueries(), &m_viewportDependentMediaQueryResults))
        return;

    TreeScope* treeScope = ScopedStyleResolver::treeScopeFor(document(), &cssSheet);
    if (!treeScope)
        return;

    treeScope->ensureScopedStyleResolver().appendCSSStyleSheet(cssSheet, *m_medium);
}

void StyleResolver::appendPendingAuthorStyleSheets()
{
    for (const auto& styleSheet : m_pendingStyleSheets)
        appendCSSStyleSheet(*styleSheet);

    m_pendingStyleSheets.clear();
    finishAppendAuthorStyleSheets();
}

void StyleResolver::appendAuthorStyleSheets(const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet>>& styleSheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    for (const auto& styleSheet : styleSheets)
        appendCSSStyleSheet(*styleSheet);
}

void StyleResolver::finishAppendAuthorStyleSheets()
{
    collectFeatures();

    if (document().layoutView() && document().layoutView()->style())
        document().layoutView()->style()->font().update(document().styleEngine().fontSelector());

    m_viewportStyleResolver->collectViewportRules();

    document().styleEngine().resetCSSFeatureFlags(m_features);
}

void StyleResolver::resetRuleFeatures()
{
    // Need to recreate RuleFeatureSet.
    m_features.clear();
    m_siblingRuleSet.clear();
    m_uncommonAttributeRuleSet.clear();
    m_needCollectFeatures = true;
}

void StyleResolver::addTreeBoundaryCrossingScope(ContainerNode& scope)
{
    m_treeBoundaryCrossingRules.addScope(scope);
}

void StyleResolver::resetAuthorStyle(TreeScope& treeScope)
{
    m_treeBoundaryCrossingRules.removeScope(treeScope.rootNode());
    resetRuleFeatures();

    ScopedStyleResolver* resolver = treeScope.scopedStyleResolver();
    if (!resolver)
        return;

    if (treeScope.rootNode().isDocumentNode()) {
        resolver->resetAuthorStyle();
        return;
    }

    // resolver is going to be freed below.
    treeScope.clearScopedStyleResolver();
}

static PassOwnPtrWillBeRawPtr<RuleSet> makeRuleSet(const WillBeHeapVector<RuleFeature>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    OwnPtrWillBeRawPtr<RuleSet> ruleSet = RuleSet::create();
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

    if (document().isTransitionDocument())
        m_features.add(defaultStyleSheets.defaultTransitionStyle()->features());

    if (m_watchedSelectorsRules)
        m_features.add(m_watchedSelectorsRules->features());

    document().styleEngine().collectScopedStyleFeaturesTo(m_features);

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
    StyleSharingList& list = styleSharingList();
    if (list.size() >= styleSharingListSize)
        list.removeLast();
    list.prepend(&element);
}

StyleSharingList& StyleResolver::styleSharingList()
{
    m_styleSharingLists.resize(styleSharingMaxDepth);

    // We never put things at depth 0 into the list since that's only the <html> element
    // and it has no siblings or cousins to share with.
    unsigned depth = std::max(std::min(m_styleSharingDepth, styleSharingMaxDepth), 1u) - 1u;

    if (!m_styleSharingLists[depth])
        m_styleSharingLists[depth] = adoptPtrWillBeNoop(new StyleSharingList);
    return *m_styleSharingLists[depth];
}

void StyleResolver::clearStyleSharingList()
{
    m_styleSharingLists.resize(0);
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
}

void StyleResolver::popParentElement(Element& parent)
{
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(&parent))
        m_selectorFilter.popParent();
}

StyleResolver::~StyleResolver()
{
}

static inline ScopedStyleResolver* scopedResolverFor(const Element* element)
{
    // Ideally, returning element->treeScope().scopedStyleResolver() should be
    // enough, but ::cue and custom pseudo elements like ::-webkit-meter-bar pierce
    // through a shadow dom boundary, yet they are not part of m_treeBoundaryCrossingRules.
    // The assumption here is that these rules only pierce through one boundary and
    // that the scope of these elements do not have a style resolver due to the fact
    // that VTT scopes and UA shadow trees don't have <style> elements. This is
    // backed up by the ASSERTs below.
    //
    // FIXME: Make ::cue and custom pseudo elements part of boundary crossing rules
    // when moving those rules to ScopedStyleResolver as part of issue 401359.

    TreeScope* treeScope = &element->treeScope();
    if (ScopedStyleResolver* resolver = treeScope->scopedStyleResolver()) {
        ASSERT(element->shadowPseudoId().isEmpty());
        ASSERT(!element->isVTTElement());
        return resolver;
    }

    treeScope = treeScope->parentTreeScope();
    if (!treeScope)
        return nullptr;
    if (element->shadowPseudoId().isEmpty() && !element->isVTTElement())
        return nullptr;
    return treeScope->scopedStyleResolver();
}

void StyleResolver::matchAuthorRules(Element* element, ElementRuleCollector& collector, bool includeEmptyRules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    CascadeOrder cascadeOrder = 0;
    WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8> resolversInShadowTree;
    collectScopedResolversForHostedShadowTrees(element, resolversInShadowTree);

    // Apply :host and :host-context rules from inner scopes.
    for (int j = resolversInShadowTree.size() - 1; j >= 0; --j)
        resolversInShadowTree.at(j)->collectMatchingShadowHostRules(collector, includeEmptyRules, ++cascadeOrder);

    // Apply normal rules from element scope.
    if (ScopedStyleResolver* resolver = scopedResolverFor(element))
        resolver->collectMatchingAuthorRules(collector, includeEmptyRules, ++cascadeOrder);

    // Apply /deep/ and ::shadow rules from outer scopes, and ::content from inner.
    m_treeBoundaryCrossingRules.collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchUARules(ElementRuleCollector& collector)
{
    collector.setMatchingUARules(true);

    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    RuleSet* userAgentStyleSheet = m_printMediaType ? defaultStyleSheets.defaultPrintStyle() : defaultStyleSheets.defaultStyle();
    matchRuleSet(collector, userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (document().inQuirksMode())
        matchRuleSet(collector, defaultStyleSheets.defaultQuirksStyle());

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (document().isViewSource())
        matchRuleSet(collector, defaultStyleSheets.defaultViewSourceStyle());

    if (document().isTransitionDocument())
        matchRuleSet(collector, defaultStyleSheets.defaultTransitionStyle());

    collector.setMatchingUARules(false);
}

void StyleResolver::matchRuleSet(ElementRuleCollector& collector, RuleSet* rules)
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
            if (isAuto) {
                state.setHasDirAutoAttribute(true);
                collector.matchedResult().addMatchedProperties(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
            }
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

PassRefPtr<LayoutStyle> StyleResolver::styleForDocument(Document& document)
{
    const LocalFrame* frame = document.frame();

    RefPtr<LayoutStyle> documentStyle = LayoutStyle::create();
    documentStyle->setRTLOrdering(document.visuallyOrdered() ? VisualOrder : LogicalOrder);
    documentStyle->setZoom(frame && !document.printing() ? frame->pageZoomFactor() : 1);
    documentStyle->setLocale(document.contentLanguage());
    documentStyle->setZIndex(0);
    documentStyle->setUserModify(document.inDesignMode() ? READ_WRITE : READ_ONLY);
    // These are designed to match the user-agent stylesheet values for the document element
    // so that the common case doesn't need to create a new LayoutStyle in
    // Document::inheritHtmlAndBodyElementStyles.
    documentStyle->setDisplay(BLOCK);
    documentStyle->setScrollBlocksOn(WebScrollBlocksOnStartTouch | WebScrollBlocksOnWheelEvent);

    document.setupFontBuilder(*documentStyle);

    return documentStyle.release();
}

AuthorStyleInfo StyleResolver::authorStyleInfo(StyleResolverState& state)
{
    const CachedUAStyle* cachedUAStyle = state.cachedUAStyle();

    if (!cachedUAStyle)
        return AuthorStyleInfo();

    // Exclude background-repeat from comparison by resetting it.
    FillLayer backgroundCopy = cachedUAStyle->backgroundLayers;
    FillLayer backgroundLayersCopy = state.style()->backgroundLayers();
    backgroundCopy.setRepeatX(NoRepeatFill);
    backgroundCopy.setRepeatY(NoRepeatFill);
    backgroundLayersCopy.setRepeatX(NoRepeatFill);
    backgroundLayersCopy.setRepeatY(NoRepeatFill);

    bool backgroundChanged = backgroundLayersCopy != backgroundCopy
        || state.style()->backgroundColor() != cachedUAStyle->backgroundColor;
    bool borderChanged = state.style()->border() != cachedUAStyle->border;

    return AuthorStyleInfo(backgroundChanged, borderChanged);
}

void StyleResolver::adjustLayoutStyle(StyleResolverState& state, Element* element)
{
    StyleAdjuster adjuster(document().inQuirksMode());
    adjuster.adjustLayoutStyle(state.mutableStyleRef(), *state.parentStyle(), element, authorStyleInfo(state));
}

// Start loading resources referenced by this style.
void StyleResolver::loadPendingResources(StyleResolverState& state)
{
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());
    document().styleEngine().fontSelector()->fontLoader()->loadPendingFonts();
}

PassRefPtr<LayoutStyle> StyleResolver::styleForElement(Element* element, LayoutStyle* defaultParent, StyleSharingBehavior sharingBehavior,
    RuleMatchingBehavior matchingBehavior)
{
    ASSERT(document().frame());
    ASSERT(document().settings());
    ASSERT(!hasPendingAuthorStyleSheets());
    ASSERT(!m_needCollectFeatures);

    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !document().isRenderingReady() && !element->layoutObject()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = LayoutStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(document().styleEngine().fontSelector());
        }

        document().setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    didAccess();

    StyleResolverParentScope::ensureParentStackIsPushed();

    ElementResolveContext elementContext(*element);

    if (sharingBehavior == AllowStyleSharing && (defaultParent || elementContext.parentStyle())) {
        SharedStyleFinder styleFinder(elementContext, m_features, m_siblingRuleSet.get(), m_uncommonAttributeRuleSet.get(), *this);
        if (RefPtr<LayoutStyle> sharedStyle = styleFinder.findSharedStyle())
            return sharedStyle.release();
    }

    StyleResolverState state(document(), elementContext, defaultParent);

    ElementAnimations* elementAnimations = element->elementAnimations();
    const LayoutStyle* baseLayoutStyle = elementAnimations ? elementAnimations->baseLayoutStyle() : nullptr;

    if (baseLayoutStyle) {
        state.setStyle(LayoutStyle::clone(*baseLayoutStyle));
        if (!state.parentStyle())
            state.setParentStyle(initialStyleForElement());
    } else {
        if (state.parentStyle()) {
            RefPtr<LayoutStyle> style = LayoutStyle::create();
            style->inheritFrom(*state.parentStyle(), isAtShadowBoundary(element) ? LayoutStyle::AtShadowBoundary : LayoutStyle::NotAtShadowBoundary);
            state.setStyle(style.release());
        } else {
            state.setStyle(initialStyleForElement());
            state.setParentStyle(LayoutStyle::clone(*state.style()));
        }
    }

    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (state.distributedToInsertionPoint()) {
        if (Element* parent = element->parentElement()) {
            if (LayoutStyle* styleOfShadowHost = parent->layoutStyle())
                state.style()->setUserModify(styleOfShadowHost->userModify());
        }
    }

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

    if (!baseLayoutStyle) {

        bool needsCollection = false;
        CSSDefaultStyleSheets::instance().ensureDefaultStyleSheetsForElement(element, needsCollection);
        if (needsCollection)
            collectFeatures();

        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());

        matchAllRules(state, collector, matchingBehavior != MatchAllRulesExcludingSMIL);

        if (element->layoutStyle() && element->layoutStyle()->textAutosizingMultiplier() != state.style()->textAutosizingMultiplier()) {
            // Preserve the text autosizing multiplier on style recalc. Autosizer will update it during layout if needed.
            // NOTE: this must occur before applyMatchedProperties for correct computation of font-relative lengths.
            state.style()->setTextAutosizingMultiplier(element->layoutStyle()->textAutosizingMultiplier());
            state.style()->setUnique();
        }

        if (state.hasDirAutoAttribute())
            state.style()->setSelfOrAncestorHasDirAutoAttribute(true);

        applyMatchedProperties(state, collector.matchedResult());
        applyCallbackSelectors(state);

        // Cache our original display.
        state.style()->setOriginalDisplay(state.style()->display());

        adjustLayoutStyle(state, element);

        if (elementAnimations)
            elementAnimations->updateBaseLayoutStyle(state.style());
    }

    // FIXME: The CSSWG wants to specify that the effects of animations are applied before
    // important rules, but this currently happens here as we require adjustment to have happened
    // before deciding which properties to transition.
    if (applyAnimatedProperties(state, element))
        adjustLayoutStyle(state, element);

    if (isHTMLBodyElement(*element))
        document().textLinkColors().setTextColor(state.style()->color());

    setAnimationUpdateIfNeeded(state, *element);

    if (state.style()->hasViewportUnits())
        document().setHasViewportUnits();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<LayoutStyle> StyleResolver::styleForKeyframe(Element& element, const LayoutStyle& elementStyle, LayoutStyle* parentStyle, const StyleRuleKeyframe* keyframe, const AtomicString& animationName)
{
    ASSERT(document().frame());
    ASSERT(document().settings());
    ASSERT(!hasPendingAuthorStyleSheets());

    StyleResolverState state(document(), &element, parentStyle);

    MatchResult result;
    result.addMatchedProperties(&keyframe->properties());

    ASSERT(!state.style());

    // Create the style
    state.setStyle(LayoutStyle::clone(elementStyle));

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    // We also don't need to bother with animation properties since the only
    // relevant one is animation-timing-function and we special-case that in
    // CSSAnimations.cpp
    bool inheritedOnly = false;
    applyMatchedProperties<HighPropertyPriority>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Now do rest of the properties.
    applyMatchedProperties<LowPropertyPriority>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    loadPendingResources(state);

    didAccess();

    return state.takeStyle();
}

// This function is used by the WebAnimations JavaScript API method animate().
// FIXME: Remove this when animate() switches away from resolution-dependent parsing.
PassRefPtrWillBeRawPtr<AnimatableValue> StyleResolver::createAnimatableValueSnapshot(Element& element, CSSPropertyID property, CSSValue* value)
{
    RefPtr<LayoutStyle> style;
    if (LayoutStyle* elementStyle = element.layoutStyle())
        style = LayoutStyle::clone(*elementStyle);
    else
        style = LayoutStyle::create();
    StyleResolverState state(element.document(), &element);
    state.setStyle(style);
    return createAnimatableValueSnapshot(state, property, value);
}

PassRefPtrWillBeRawPtr<AnimatableValue> StyleResolver::createAnimatableValueSnapshot(StyleResolverState& state, CSSPropertyID property, CSSValue* value)
{
    if (value) {
        StyleBuilder::applyProperty(property, state, value);
        state.fontBuilder().createFont(state.document().styleEngine().fontSelector(), state.mutableStyleRef());
    }
    return CSSAnimatableValueFactory::create(property, *state.style());
}

PassRefPtrWillBeRawPtr<PseudoElement> StyleResolver::createPseudoElement(Element* parent, PseudoId pseudoId)
{
    if (pseudoId == FIRST_LETTER)
        return FirstLetterPseudoElement::create(parent);
    return PseudoElement::create(parent, pseudoId);
}

PassRefPtrWillBeRawPtr<PseudoElement> StyleResolver::createPseudoElementIfNeeded(Element& parent, PseudoId pseudoId)
{
    LayoutObject* parentRenderer = parent.layoutObject();
    if (!parentRenderer)
        return nullptr;

    // The first letter pseudo element has to look up the tree and see if any
    // of the ancestors are first letter.
    if (pseudoId < FIRST_INTERNAL_PSEUDOID && pseudoId != FIRST_LETTER && !parentRenderer->style()->hasPseudoStyle(pseudoId))
        return nullptr;

    if (pseudoId == BACKDROP && !parent.isInTopLayer())
        return nullptr;

    if (pseudoId == FIRST_LETTER && (parent.isSVGElement() || !FirstLetterPseudoElement::firstLetterTextRenderer(parent)))
        return nullptr;

    if (!parentRenderer->canHaveGeneratedChildren())
        return nullptr;

    LayoutStyle* parentStyle = parentRenderer->style();
    if (LayoutStyle* cachedStyle = parentStyle->getCachedPseudoStyle(pseudoId)) {
        if (!pseudoElementRendererIsNeeded(cachedStyle))
            return nullptr;
        return createPseudoElement(&parent, pseudoId);
    }

    StyleResolverState state(document(), &parent, parentStyle);
    if (!pseudoStyleForElementInternal(parent, pseudoId, parentStyle, state))
        return nullptr;
    RefPtr<LayoutStyle> style = state.takeStyle();
    ASSERT(style);
    parentStyle->addCachedPseudoStyle(style);

    if (!pseudoElementRendererIsNeeded(style.get()))
        return nullptr;

    RefPtrWillBeRawPtr<PseudoElement> pseudo = createPseudoElement(&parent, pseudoId);

    setAnimationUpdateIfNeeded(state, *pseudo);
    if (ElementAnimations* elementAnimations = pseudo->elementAnimations())
        elementAnimations->cssAnimations().maybeApplyPendingUpdate(pseudo.get());
    return pseudo.release();
}

bool StyleResolver::pseudoStyleForElementInternal(Element& element, const PseudoStyleRequest& pseudoStyleRequest, LayoutStyle* parentStyle, StyleResolverState& state)
{
    ASSERT(document().frame());
    ASSERT(document().settings());
    ASSERT(pseudoStyleRequest.pseudoId != FIRST_LINE_INHERITED);
    ASSERT(state.parentStyle());

    StyleResolverParentScope::ensureParentStackIsPushed();

    Element* pseudoElement = element.pseudoElement(pseudoStyleRequest.pseudoId);

    ElementAnimations* elementAnimations = pseudoElement ? pseudoElement->elementAnimations() : nullptr;
    const LayoutStyle* baseLayoutStyle = elementAnimations ? elementAnimations->baseLayoutStyle() : nullptr;

    if (baseLayoutStyle) {
        state.setStyle(LayoutStyle::clone(*baseLayoutStyle));
    } else if (pseudoStyleRequest.allowsInheritance(state.parentStyle())) {
        RefPtr<LayoutStyle> style = LayoutStyle::create();
        style->inheritFrom(*state.parentStyle());
        state.setStyle(style.release());
    } else {
        state.setStyle(initialStyleForElement());
        state.setParentStyle(LayoutStyle::clone(*state.style()));
    }

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    // Since we don't use pseudo-elements in any of our quirk/print
    // user agent rules, don't waste time walking those rules.

    if (!baseLayoutStyle) {
        // Check UA, user and author rules.
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
        collector.setPseudoStyleRequest(pseudoStyleRequest);

        matchUARules(collector);
        matchAuthorRules(state.element(), collector, false);

        if (collector.matchedResult().matchedProperties.isEmpty())
            return false;

        applyMatchedProperties(state, collector.matchedResult());
        applyCallbackSelectors(state);

        // Cache our original display.
        state.style()->setOriginalDisplay(state.style()->display());

        // FIXME: Passing 0 as the Element* introduces a lot of complexity
        // in the adjustLayoutStyle code.
        adjustLayoutStyle(state, 0);

        if (elementAnimations)
            elementAnimations->updateBaseLayoutStyle(state.style());
    }

    // FIXME: The CSSWG wants to specify that the effects of animations are applied before
    // important rules, but this currently happens here as we require adjustment to have happened
    // before deciding which properties to transition.
    if (applyAnimatedProperties(state, pseudoElement))
        adjustLayoutStyle(state, 0);

    didAccess();

    if (state.style()->hasViewportUnits())
        document().setHasViewportUnits();

    return true;
}

PassRefPtr<LayoutStyle> StyleResolver::pseudoStyleForElement(Element* element, const PseudoStyleRequest& pseudoStyleRequest, LayoutStyle* parentStyle)
{
    ASSERT(parentStyle);
    if (!element)
        return nullptr;

    StyleResolverState state(document(), element, parentStyle);
    if (!pseudoStyleForElementInternal(*element, pseudoStyleRequest, parentStyle, state)) {
        if (pseudoStyleRequest.type == PseudoStyleRequest::ForRenderer)
            return nullptr;
        return state.takeStyle();
    }

    if (PseudoElement* pseudoElement = element->pseudoElement(pseudoStyleRequest.pseudoId))
        setAnimationUpdateIfNeeded(state, *pseudoElement);

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<LayoutStyle> StyleResolver::styleForPage(int pageIndex)
{
    ASSERT(!hasPendingAuthorStyleSheets());
    StyleResolverState state(document(), document().documentElement()); // m_rootElementStyle will be set to the document style.

    RefPtr<LayoutStyle> style = LayoutStyle::create();
    const LayoutStyle* rootElementStyle = state.rootElementStyle() ? state.rootElementStyle() : document().layoutStyle();
    ASSERT(rootElementStyle);
    style->inheritFrom(*rootElementStyle);
    state.setStyle(style.release());

    PageRuleCollector collector(rootElementStyle, pageIndex);

    collector.matchPageRules(CSSDefaultStyleSheets::instance().defaultPrintStyle());

    if (ScopedStyleResolver* scopedResolver = document().scopedStyleResolver())
        scopedResolver->matchPageRules(collector);

    bool inheritedOnly = false;

    MatchResult& result = collector.matchedResult();
    applyMatchedProperties<HighPropertyPriority>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    applyMatchedProperties<LowPropertyPriority>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    loadPendingResources(state);

    didAccess();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<LayoutStyle> StyleResolver::initialStyleForElement()
{
    RefPtr<LayoutStyle> style = LayoutStyle::create();
    FontBuilder fontBuilder(document());
    fontBuilder.setInitial(style->effectiveZoom());
    fontBuilder.createFont(document().styleEngine().fontSelector(), *style);
    return style.release();
}

PassRefPtr<LayoutStyle> StyleResolver::styleForText(Text* textNode)
{
    ASSERT(textNode);

    Node* parentNode = NodeRenderingTraversal::parent(*textNode);
    if (!parentNode || !parentNode->layoutStyle())
        return initialStyleForElement();
    return parentNode->layoutStyle();
}

void StyleResolver::updateFont(StyleResolverState& state)
{
    state.fontBuilder().createFont(document().styleEngine().fontSelector(), state.mutableStyleRef());
    state.setConversionFontSizes(CSSToLengthConversionData::FontSizes(state.style(), state.rootElementStyle()));
    state.setConversionZoom(state.style()->effectiveZoom());
}

PassRefPtrWillBeRawPtr<StyleRuleList> StyleResolver::styleRulesForElement(Element* element, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingStyleRules);
    collectPseudoRulesForElement(element, collector, NOPSEUDO, rulesToInclude);
    return collector.matchedStyleRuleList();
}

PassRefPtrWillBeRawPtr<CSSRuleList> StyleResolver::pseudoCSSRulesForElement(Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingCSSRules);
    collectPseudoRulesForElement(element, collector, pseudoId, rulesToInclude);
    return collector.matchedCSSRuleList();
}

PassRefPtrWillBeRawPtr<CSSRuleList> StyleResolver::cssRulesForElement(Element* element, unsigned rulesToInclude)
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

bool StyleResolver::applyAnimatedProperties(StyleResolverState& state, const Element* animatingElement)
{
    Element* element = state.element();
    ASSERT(element);

    // The animating element may be this element, or its pseudo element. It is
    // null when calculating the style for a potential pseudo element that has
    // yet to be created.
    ASSERT(animatingElement == element || !animatingElement || animatingElement->parentOrShadowHostElement() == element);

    if (!(animatingElement && animatingElement->hasAnimations())
        && !state.style()->transitions() && !state.style()->animations())
        return false;

    state.setAnimationUpdate(CSSAnimations::calculateUpdate(animatingElement, *element, *state.style(), state.parentStyle(), this));
    if (!state.animationUpdate())
        return false;

    const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& activeInterpolationsForAnimations = state.animationUpdate()->activeInterpolationsForAnimations();
    const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& activeInterpolationsForTransitions = state.animationUpdate()->activeInterpolationsForTransitions();
    applyAnimatedProperties<HighPropertyPriority>(state, activeInterpolationsForAnimations);
    applyAnimatedProperties<HighPropertyPriority>(state, activeInterpolationsForTransitions);

    updateFont(state);

    applyAnimatedProperties<LowPropertyPriority>(state, activeInterpolationsForAnimations);
    applyAnimatedProperties<LowPropertyPriority>(state, activeInterpolationsForTransitions);

    // Start loading resources used by animations.
    loadPendingResources(state);

    ASSERT(!state.fontBuilder().fontDirty());

    return true;
}

StyleRuleKeyframes* StyleResolver::findKeyframesRule(const Element* element, const AtomicString& animationName)
{
    WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8> resolvers;
    collectScopedResolversForHostedShadowTrees(element, resolvers);
    if (ScopedStyleResolver* scopedResolver = element->treeScope().scopedStyleResolver())
        resolvers.append(scopedResolver);

    for (size_t i = 0; i < resolvers.size(); ++i) {
        if (StyleRuleKeyframes* keyframesRule = resolvers[i]->keyframeStylesForAnimation(animationName.impl()))
            return keyframesRule;
    }
    return nullptr;
}

template <CSSPropertyPriority priority>
void StyleResolver::applyAnimatedProperties(StyleResolverState& state, const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& activeInterpolations)
{
    for (const auto& interpolationEntry : activeInterpolations) {
        CSSPropertyID property = interpolationEntry.key;
        if (!CSSPropertyPriorityData<priority>::propertyHasPriority(property))
            continue;
        const StyleInterpolation* interpolation = toStyleInterpolation(interpolationEntry.value.get());
        interpolation->apply(state);
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
    case CSSPropertyFontStretch:
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

static inline bool isValidFirstLetterStyleProperty(CSSPropertyID id)
{
    switch (id) {
    // Valid ::first-letter properties listed in spec:
    // http://www.w3.org/TR/css3-selectors/#application-in-css
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
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
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColor:
    case CSSPropertyFloat:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontKerning:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontWeight:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyTextTransform:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWordSpacing:
        return true;
    case CSSPropertyTextDecoration:
        ASSERT(!RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return true;
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
        ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return true;

    // text-shadow added in text decoration spec:
    // http://www.w3.org/TR/css-text-decor-3/#text-shadow-property
    case CSSPropertyTextShadow:
    // box-shadox added in CSS3 backgrounds spec:
    // http://www.w3.org/TR/css3-background/#placement
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
    // Properties that we currently support outside of spec.
    case CSSPropertyWebkitLineBoxContain:
    case CSSPropertyVisibility:
        return true;

    default:
        return false;
    }
}

// This method expands the 'all' shorthand property to longhand properties
// and applies the expanded longhand properties.
template <CSSPropertyPriority priority>
void StyleResolver::applyAllProperty(StyleResolverState& state, CSSValue* allValue, bool inheritedOnly)
{
    bool isUnsetValue = !allValue->isInitialValue() && !allValue->isInheritedValue();
    unsigned startCSSProperty = CSSPropertyPriorityData<priority>::first();
    unsigned endCSSProperty = CSSPropertyPriorityData<priority>::last();

    for (unsigned i = startCSSProperty; i <= endCSSProperty; ++i) {
        CSSPropertyID propertyId = static_cast<CSSPropertyID>(i);

        // StyleBuilder does not allow any expanded shorthands.
        if (isShorthandProperty(propertyId))
            continue;

        // all shorthand spec says:
        // The all property is a shorthand that resets all CSS properties
        // except direction and unicode-bidi.
        // c.f. http://dev.w3.org/csswg/css-cascade/#all-shorthand
        // We skip applyProperty when a given property is unicode-bidi or
        // direction.
        if (!CSSProperty::isAffectedByAllProperty(propertyId))
            continue;

        // When hitting matched properties' cache, only inherited properties will be
        // applied.
        if (inheritedOnly && !CSSPropertyMetadata::isInheritedProperty(propertyId))
            continue;

        CSSValue* value;
        if (!isUnsetValue) {
            value = allValue;
        } else {
            if (CSSPropertyMetadata::isInheritedProperty(propertyId))
                value = cssValuePool().createInheritedValue().get();
            else
                value = cssValuePool().createExplicitInitialValue().get();
        }
        StyleBuilder::applyProperty(propertyId, state, value);
    }
}

template <CSSPropertyPriority priority>
void StyleResolver::applyProperties(StyleResolverState& state, const StylePropertySet* properties, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType)
{
    unsigned propertyCount = properties->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        StylePropertySet::PropertyReference current = properties->propertyAt(i);
        if (isImportant != current.isImportant())
            continue;

        CSSPropertyID property = current.id();
        if (property == CSSPropertyAll) {
            applyAllProperty<priority>(state, current.value(), inheritedOnly);
            continue;
        }

        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(property))
            continue;
        if (propertyWhitelistType == PropertyWhitelistFirstLetter && !isValidFirstLetterStyleProperty(property))
            continue;

        if (inheritedOnly && !current.isInherited()) {
            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }

        if (!CSSPropertyPriorityData<priority>::propertyHasPriority(property))
            continue;

        StyleBuilder::applyProperty(current.id(), state, current.value());
    }
}

template <CSSPropertyPriority priority>
void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult, bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    if (state.style()->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
            unsigned linkMatchType = matchedProperties.m_types.linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            state.setApplyPropertyToRegularStyle(linkMatchType & SelectorChecker::MatchLink);
            state.setApplyPropertyToVisitedLinkStyle(linkMatchType & SelectorChecker::MatchVisited);

            applyProperties<priority>(state, matchedProperties.properties.get(), isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.m_types.whitelistType));
        }
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        applyProperties<priority>(state, matchedProperties.properties.get(), isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.m_types.whitelistType));
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
    m_viewportStyleResolver->collectViewportRules();
    m_matchedPropertiesCache.clearViewportDependent();
}

void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult)
{
    const Element* element = state.element();
    ASSERT(element);

    INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyApply);

    unsigned cacheHash = matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const CachedMatchedProperties* cachedMatchedProperties = cacheHash ? m_matchedPropertiesCache.find(cacheHash, state, matchResult) : 0;

    if (cachedMatchedProperties && MatchedPropertiesCache::isCacheable(element, *state.style(), *state.parentStyle())) {
        INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheHit);
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFromCached(*cachedMatchedProperties->layoutStyle);
        if (state.parentStyle()->inheritedDataShared(*cachedMatchedProperties->parentLayoutStyle) && !isAtShadowBoundary(element)
            && (!state.distributedToInsertionPoint() || state.style()->userModify() == READ_ONLY)) {
            INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheInheritedHit);

            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(*cachedMatchedProperties->layoutStyle);

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);

            updateFont(state);

            return;
        }
        applyInheritedOnly = true;
    }

    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    applyMatchedProperties<HighPropertyPriority>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<HighPropertyPriority>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<HighPropertyPriority>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (UNLIKELY(isSVGForeignObjectElement(element))) {
        // LayoutSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should not be scaled again.
        //
        // FIXME: The following hijacks the zoom property for foreignObject so that children of foreignObject get the
        // correct font-size in case of zooming. 'zoom' has HighPropertyPriority, along with other font-related
        // properties used as input to the FontBuilder, so resetting it here may cause the FontBuilder to recompute the
        // font used as inheritable font for foreignObject content. If we want to support zoom on foreignObject we'll
        // need to find another way of handling the SVG zoom model.
        state.setEffectiveZoom(LayoutStyle::initialZoom());
    }

    if (cachedMatchedProperties && cachedMatchedProperties->layoutStyle->effectiveZoom() != state.style()->effectiveZoom()) {
        state.fontBuilder().didChangeEffectiveZoom();
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cachedMatchedProperties && cachedMatchedProperties->layoutStyle->fontDescription() != state.style()->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyMatchedProperties<LowPropertyPriority>(state, matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Cache the UA properties to pass them to LayoutTheme in adjustLayoutStyle.
    state.cacheUserAgentBorderAndBackground();

    // Now do the author and user normal priority properties and all the !important properties.
    applyMatchedProperties<LowPropertyPriority>(state, matchResult, false, matchResult.ranges.lastUARule + 1, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<LowPropertyPriority>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<LowPropertyPriority>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    loadPendingResources(state);

    if (!cachedMatchedProperties && cacheHash && MatchedPropertiesCache::isCacheable(element, *state.style(), *state.parentStyle())) {
        INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheAdded);
        m_matchedPropertiesCache.add(*state.style(), *state.parentStyle(), cacheHash, matchResult);
    }

    ASSERT(!state.fontBuilder().fontDirty());
}

void StyleResolver::applyCallbackSelectors(StyleResolverState& state)
{
    if (!m_watchedSelectorsRules)
        return;

    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingStyleRules);

    MatchRequest matchRequest(m_watchedSelectorsRules.get(), true);
    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
    collector.collectMatchingRules(matchRequest, ruleRange);
    collector.sortAndTransferMatchedRules();

    RefPtrWillBeRawPtr<StyleRuleList> rules = collector.matchedStyleRuleList();
    if (!rules)
        return;
    for (size_t i = 0; i < rules->m_list.size(); i++)
        state.style()->addCallbackSelector(rules->m_list[i]->selectorList().selectorsText());
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
    fprintf(stderr, "=== Style Resolver Stats (resolve #%u) (%s) ===\n", ++m_styleResolverStatsSequence, document().url().string().utf8().data());
    fprintf(stderr, "%s\n", m_styleResolverStats->report().utf8().data());
    fprintf(stderr, "== Totals ==\n");
    fprintf(stderr, "%s\n", m_styleResolverStatsTotals->report().utf8().data());
}

void StyleResolver::applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, LayoutStyle* style)
{
    StyleResolverState state(document(), document().documentElement(), style);
    state.setStyle(style);

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
        if (m_medium->eval(m_viewportDependentMediaQueryResults[i]->expression()) != m_viewportDependentMediaQueryResults[i]->result())
            return true;
    }
    return false;
}

DEFINE_TRACE(StyleResolver)
{
#if ENABLE(OILPAN)
    visitor->trace(m_matchedPropertiesCache);
    visitor->trace(m_viewportDependentMediaQueryResults);
    visitor->trace(m_selectorFilter);
    visitor->trace(m_viewportStyleResolver);
    visitor->trace(m_features);
    visitor->trace(m_siblingRuleSet);
    visitor->trace(m_uncommonAttributeRuleSet);
    visitor->trace(m_watchedSelectorsRules);
    visitor->trace(m_treeBoundaryCrossingRules);
    visitor->trace(m_styleResourceLoader);
    visitor->trace(m_styleSharingLists);
    visitor->trace(m_pendingStyleSheets);
    visitor->trace(m_document);
#endif
}

} // namespace blink
