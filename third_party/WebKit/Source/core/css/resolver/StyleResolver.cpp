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

// FIXME: This include list is way more than we need!
#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "XMLNames.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/Animation.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/DeprecatedStyleBuilder.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/FontFeatureValue.h"
#include "core/css/FontSize.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/Pair.h"
#include "core/css/RuleSet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StylePropertyShorthand.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/MatchResult.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/FullscreenController.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/Text.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/track/WebVTTElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/cache/CachedDocument.h"
#include "core/loader/cache/CachedSVGDocumentReference.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/LinkHash.h"
#include "core/platform/graphics/filters/custom/CustomFilterConstants.h"
#include "core/platform/text/LocaleToScriptMapping.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/StyleCachedImage.h"
#include "core/rendering/style/StyleCachedImageSet.h"
#include "core/rendering/style/StyleCustomFilterProgramCache.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFontFaceElement.h"
#include "wtf/MemoryInstrumentationHashMap.h"
#include "wtf/MemoryInstrumentationVector.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

using namespace std;

namespace WTF {

template<> struct SequenceMemoryInstrumentationTraits<const WebCore::RuleData*> {
    template <typename I> static void reportMemoryUsage(I, I, MemoryClassInfo&) { }
};

}

namespace WebCore {

using namespace HTMLNames;

RenderStyle* StyleResolver::s_styleNotYetAvailable;

static StylePropertySet* leftToRightDeclaration()
{
    DEFINE_STATIC_LOCAL(RefPtr<MutableStylePropertySet>, leftToRightDecl, (MutableStylePropertySet::create()));
    if (leftToRightDecl->isEmpty())
        leftToRightDecl->setProperty(CSSPropertyDirection, CSSValueLtr);
    return leftToRightDecl.get();
}

static StylePropertySet* rightToLeftDeclaration()
{
    DEFINE_STATIC_LOCAL(RefPtr<MutableStylePropertySet>, rightToLeftDecl, (MutableStylePropertySet::create()));
    if (rightToLeftDecl->isEmpty())
        rightToLeftDecl->setProperty(CSSPropertyDirection, CSSValueRtl);
    return rightToLeftDecl.get();
}

StyleResolver::StyleResolver(Document* document, bool matchAuthorAndUserStyles)
    : m_document(document)
    , m_matchAuthorAndUserStyles(matchAuthorAndUserStyles)
    , m_fontSelector(CSSFontSelector::create(document))
    , m_viewportStyleResolver(ViewportStyleResolver::create(document))
    , m_styleBuilder(DeprecatedStyleBuilder::sharedStyleBuilder())
    , m_styleResourceLoader(document->cachedResourceLoader())
    , m_elementAffectedByClassRules(false)
{
    Element* root = document->documentElement();

    CSSDefaultStyleSheets::initDefaultStyle(root);

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = document->view();
    if (view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType()));
    else
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, DisallowStyleSharing, MatchOnlyUserAgentRules);

    if (m_rootDefaultStyle && view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType(), view->frame(), m_rootDefaultStyle.get()));

    m_styleTree.clear();

    DocumentStyleSheetCollection* styleSheetCollection = document->styleSheetCollection();
    m_ruleSets.initUserStyle(styleSheetCollection, *m_medium, *this);

#if ENABLE(SVG_FONTS)
    if (document->svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = document->svgExtensions()->svgFontFaceElements();
        HashSet<SVGFontFaceElement*>::const_iterator end = svgFontFaceElements.end();
        for (HashSet<SVGFontFaceElement*>::const_iterator it = svgFontFaceElements.begin(); it != end; ++it)
            fontSelector()->addFontFaceRule((*it)->fontFaceRule());
    }
#endif

    // FIXME: Stylesheet candidate nodes are sorted in document order, but scoping nodes are not sorted.
    appendAuthorStyleSheets(0, styleSheetCollection->activeAuthorStyleSheets());
}

void StyleResolver::appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >& styleSheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i) {
        CSSStyleSheet* cssSheet = styleSheets[i].get();
        ASSERT(!cssSheet->disabled());
        if (cssSheet->mediaQueries() && !m_medium->eval(cssSheet->mediaQueries(), this))
            continue;

        StyleSheetContents* sheet = cssSheet->contents();
        ScopedStyleResolver* resolver = ensureScopedStyleResolver(ScopedStyleResolver::scopingNodeFor(cssSheet));
        ASSERT(resolver);
        resolver->addRulesFromSheet(sheet, *m_medium, this);
        m_inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet);
    }

    collectFeatures();

    if (document()->renderer() && document()->renderer()->style())
        document()->renderer()->style()->font().update(fontSelector());

    if (RuntimeEnabledFeatures::cssViewportEnabled())
        viewportStyleResolver()->resolve();
}

void StyleResolver::resetAuthorStyle()
{
    m_styleTree.clear();
}

void StyleResolver::resetAuthorStyle(const ContainerNode* scopingNode)
{
    m_styleTree.clear();
    ScopedStyleResolver* resolver = scopingNode ? m_styleTree.scopedStyleResolverFor(scopingNode) : m_styleTree.scopedStyleResolverForDocument();
    if (!resolver)
        return;

    m_ruleSets.shadowDistributedRules().reset(scopingNode);

    resolver->resetAuthorStyle();

    if (!scopingNode || !resolver->hasOnlyEmptyRuleSets())
        return;

    m_styleTree.remove(scopingNode);
}

void StyleResolver::resetAtHostRules(const ContainerNode* scopingNode)
{
    ASSERT(scopingNode);
    ASSERT(scopingNode->isShadowRoot());

    const ShadowRoot* shadowRoot = toShadowRoot(scopingNode);
    const ContainerNode* shadowHost = shadowRoot->shadowHost();
    ScopedStyleResolver* resolver = m_styleTree.scopedStyleResolverFor(shadowHost);
    if (!resolver)
        return;

    resolver->resetAtHostRules(shadowRoot);
    if (!resolver->hasOnlyEmptyRuleSets())
        return;

    m_styleTree.remove(shadowHost);
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
    m_ruleSets.collectFeaturesTo(m_features, document()->isViewSource());
    m_styleTree.collectFeaturesTo(m_features);

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);
}

void StyleResolver::pushParentElement(Element* parent)
{
    const ContainerNode* parentsParent = parent->parentOrShadowHostElement();

    // We are not always invoked consistently. For example, script execution can cause us to enter
    // style recalc in the middle of tree building. We may also be invoked from somewhere within the tree.
    // Reset the stack in this case, or if we see a new root element.
    // Otherwise just push the new parent.
    if (!parentsParent || m_selectorFilter.parentStackIsEmpty())
        m_selectorFilter.setupParentStack(parent);
    else
        m_selectorFilter.pushParent(parent);

    // Note: We mustn't skip ShadowRoot nodes for the scope stack.
    m_styleTree.pushStyleCache(parent, parent->parentOrShadowHostNode());
}

void StyleResolver::popParentElement(Element* parent)
{
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(parent))
        m_selectorFilter.popParent();

    m_styleTree.popStyleCache(parent);
}

void StyleResolver::pushParentShadowRoot(const ShadowRoot* shadowRoot)
{
    ASSERT(shadowRoot->host());
    m_styleTree.pushStyleCache(shadowRoot, shadowRoot->host());
}

void StyleResolver::popParentShadowRoot(const ShadowRoot* shadowRoot)
{
    ASSERT(shadowRoot->host());
    m_styleTree.popStyleCache(shadowRoot);
}

// This is a simplified style setting function for keyframe styles
void StyleResolver::addKeyframeStyle(PassRefPtr<StyleRuleKeyframes> rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), rule);
}

StyleResolver::~StyleResolver()
{
    m_fontSelector->clearDocument();
    m_viewportStyleResolver->clearDocument();
}

bool StyleResolver::classNamesAffectedByRules(const SpaceSplitString& classNames) const
{
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (m_features.classesInRules.contains(classNames[i].impl()))
            return true;
    }
    return false;
}

inline void StyleResolver::matchShadowDistributedRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_ruleSets.shadowDistributedRules().isEmpty())
        return;

    bool previousCanUseFastReject = collector.canUseFastReject();
    SelectorChecker::BehaviorAtBoundary previousBoundary = collector.behaviorAtBoundary();
    collector.setBehaviorAtBoundary(static_cast<SelectorChecker::BehaviorAtBoundary>(SelectorChecker::CrossesBoundary | SelectorChecker::ScopeContainsLastMatchedElement));
    collector.setCanUseFastReject(false);

    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;
    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();

    Vector<MatchRequest> matchRequests;
    m_ruleSets.shadowDistributedRules().collectMatchRequests(includeEmptyRules, matchRequests);
    for (size_t i = 0; i < matchRequests.size(); ++i)
        collector.collectMatchingRules(matchRequests[i], ruleRange);
    collector.sortAndTransferMatchedRules();

    collector.setBehaviorAtBoundary(previousBoundary);
    collector.setCanUseFastReject(previousCanUseFastReject);
}

void StyleResolver::matchHostRules(ScopedStyleResolver* resolver, ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_state.element() != resolver->scopingNode())
        return;
    resolver->matchHostRules(collector, includeEmptyRules);
}

void StyleResolver::matchScopedAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    // fast path
    if (m_styleTree.hasOnlyScopedResolverForDocument()) {
        m_styleTree.scopedStyleResolverForDocument()->matchAuthorRules(collector, includeEmptyRules, true);
        return;
    }

    Vector<ScopedStyleResolver*, 8> stack;
    m_styleTree.resolveScopedStyles(m_state.element(), stack);
    if (stack.isEmpty())
        return;

    bool applyAuthorStyles = m_state.element()->treeScope()->applyAuthorStyles();
    for (int i = stack.size() - 1; i >= 0; --i)
        stack.at(i)->matchAuthorRules(collector, includeEmptyRules, applyAuthorStyles);

    matchHostRules(stack.first(), collector, includeEmptyRules);
}

void StyleResolver::matchAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    matchScopedAuthorRules(collector, includeEmptyRules);
    matchShadowDistributedRules(collector, includeEmptyRules);
}

void StyleResolver::matchUserRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (!m_ruleSets.userStyle())
        return;

    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUserRule = collector.matchedResult().matchedProperties.size() - 1;

    MatchRequest matchRequest(m_ruleSets.userStyle(), includeEmptyRules);
    RuleRange ruleRange = collector.matchedResult().ranges.userRuleRange();
    collector.collectMatchingRules(matchRequest, ruleRange);
    collector.collectMatchingRulesForRegion(matchRequest, ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchUARules(ElementRuleCollector& collector)
{
    collector.setMatchingUARules(true);

    // First we match rules from the user agent sheet.
    if (CSSDefaultStyleSheets::simpleDefaultStyleSheet)
        collector.matchedResult().isCacheable = false;

    RuleSet* userAgentStyleSheet = m_medium->mediaTypeMatchSpecific("print")
        ? CSSDefaultStyleSheets::defaultPrintStyle : CSSDefaultStyleSheets::defaultStyle;
    matchUARules(collector, userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (document()->inQuirksMode())
        matchUARules(collector, CSSDefaultStyleSheets::defaultQuirksStyle);

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (document()->isViewSource())
        matchUARules(collector, CSSDefaultStyleSheets::viewSourceStyle());

    collector.setMatchingUARules(false);
}

void StyleResolver::matchUARules(ElementRuleCollector& collector, RuleSet* rules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUARule = collector.matchedResult().matchedProperties.size() - 1;

    RuleRange ruleRange = collector.matchedResult().ranges.UARuleRange();
    collector.collectMatchingRules(MatchRequest(rules), ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchAllRules(ElementRuleCollector& collector, bool matchAuthorAndUserStyles, bool includeSMILProperties)
{
    matchUARules(collector);

    // Now we check user sheet rules.
    if (matchAuthorAndUserStyles)
        matchUserRules(collector, false);

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (m_state.element()->isStyledElement()) {
        collector.addElementStyleProperties(m_state.element()->presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        collector.addElementStyleProperties(m_state.element()->additionalPresentationAttributeStyle());

        if (m_state.element()->isHTMLElement()) {
            bool isAuto;
            TextDirection textDirection = toHTMLElement(m_state.element())->directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                collector.matchedResult().addMatchedProperties(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }

    // Check the rules in author sheets next.
    if (matchAuthorAndUserStyles)
        matchAuthorRules(collector, false);

    if (m_state.element()->isStyledElement()) {
        // Now check our inline style attribute.
        if (matchAuthorAndUserStyles && m_state.element()->inlineStyle()) {
            // Inline style is immutable as long as there is no CSSOM wrapper.
            // FIXME: Media control shadow trees seem to have problems with caching.
            bool isInlineStyleCacheable = !m_state.element()->inlineStyle()->isMutable() && !m_state.element()->isInShadowTree();
            // FIXME: Constify.
            collector.addElementStyleProperties(m_state.element()->inlineStyle(), isInlineStyleCacheable);
        }

        // Now check SMIL animation override style.
        if (includeSMILProperties && matchAuthorAndUserStyles && m_state.element()->isSVGElement())
            collector.addElementStyleProperties(toSVGElement(m_state.element())->animatedSMILStyleProperties(), false /* isCacheable */);

        if (m_state.element()->hasActiveAnimations())
            collector.matchedResult().isCacheable = false;
    }
}

static const unsigned cStyleSearchThreshold = 10;
static const unsigned cStyleSearchLevelThreshold = 10;

static inline bool parentElementPreventsSharing(const Element* parentElement)
{
    if (!parentElement)
        return false;
    return parentElement->hasFlagsSetDuringStylingOfChildren();
}

Node* StyleResolver::locateCousinList(Element* parent, unsigned& visitedNodeCount) const
{
    if (visitedNodeCount >= cStyleSearchThreshold * cStyleSearchLevelThreshold)
        return 0;
    if (!parent || !parent->isStyledElement())
        return 0;
    if (parent->hasScopedHTMLStyleChild())
        return 0;
    if (parent->inlineStyle())
        return 0;
    if (parent->isSVGElement() && toSVGElement(parent)->animatedSMILStyleProperties())
        return 0;
    if (parent->hasID() && m_features.idsInRules.contains(parent->idForStyleResolution().impl()))
        return 0;

    RenderStyle* parentStyle = parent->renderStyle();
    unsigned subcount = 0;
    Node* thisCousin = parent;
    Node* currentNode = parent->previousSibling();

    // Reserve the tries for this level. This effectively makes sure that the algorithm
    // will never go deeper than cStyleSearchLevelThreshold levels into recursion.
    visitedNodeCount += cStyleSearchThreshold;
    while (thisCousin) {
        while (currentNode) {
            ++subcount;
            if (!currentNode->hasScopedHTMLStyleChild() && currentNode->renderStyle() == parentStyle && currentNode->lastChild()
                && currentNode->isElementNode() && !parentElementPreventsSharing(toElement(currentNode))
                && !toElement(currentNode)->shadow()
                ) {
                // Adjust for unused reserved tries.
                visitedNodeCount -= cStyleSearchThreshold - subcount;
                return currentNode->lastChild();
            }
            if (subcount >= cStyleSearchThreshold)
                return 0;
            currentNode = currentNode->previousSibling();
        }
        currentNode = locateCousinList(thisCousin->parentElement(), visitedNodeCount);
        thisCousin = currentNode;
    }

    return 0;
}

bool StyleResolver::styleSharingCandidateMatchesRuleSet(RuleSet* ruleSet)
{
    if (!ruleSet)
        return false;

    ElementRuleCollector collector(m_state.elementContext(), m_selectorFilter, m_state.style(), m_inspectorCSSOMWrappers);
    return collector.hasAnyMatchingRules(ruleSet);
}

bool StyleResolver::canShareStyleWithControl(const ElementResolveContext& context, Element* element) const
{
    if (!element->hasTagName(inputTag) || !context.element()->hasTagName(inputTag))
        return false;

    HTMLInputElement* thisInputElement = toHTMLInputElement(element);
    HTMLInputElement* otherInputElement = toHTMLInputElement(context.element());
    if (thisInputElement->elementData() != otherInputElement->elementData()) {
        if (thisInputElement->fastGetAttribute(typeAttr) != otherInputElement->fastGetAttribute(typeAttr))
            return false;
        if (thisInputElement->fastGetAttribute(readonlyAttr) != otherInputElement->fastGetAttribute(readonlyAttr))
            return false;
    }

    if (thisInputElement->isAutofilled() != otherInputElement->isAutofilled())
        return false;
    if (thisInputElement->shouldAppearChecked() != otherInputElement->shouldAppearChecked())
        return false;
    if (thisInputElement->shouldAppearIndeterminate() != otherInputElement->shouldAppearIndeterminate())
        return false;
    if (thisInputElement->isRequired() != otherInputElement->isRequired())
        return false;

    if (element->isDisabledFormControl() != context.element()->isDisabledFormControl())
        return false;

    if (element->isDefaultButtonForForm() != context.element()->isDefaultButtonForForm())
        return false;

    if (context.document()->containsValidityStyleRules()) {
        bool willValidate = element->willValidate();

        if (willValidate != context.element()->willValidate())
            return false;

        if (willValidate && (element->isValidFormControlElement() != context.element()->isValidFormControlElement()))
            return false;

        if (element->isInRange() != context.element()->isInRange())
            return false;

        if (element->isOutOfRange() != context.element()->isOutOfRange())
            return false;
    }

    return true;
}

static inline bool elementHasDirectionAuto(Element* element)
{
    // FIXME: This line is surprisingly hot, we may wish to inline hasDirectionAuto into StyleResolver.
    return element->isHTMLElement() && toHTMLElement(element)->hasDirectionAuto();
}

bool StyleResolver::sharingCandidateHasIdenticalStyleAffectingAttributes(const ElementResolveContext& context, Element* sharingCandidate) const
{
    if (context.element()->elementData() == sharingCandidate->elementData())
        return true;
    if (context.element()->fastGetAttribute(XMLNames::langAttr) != sharingCandidate->fastGetAttribute(XMLNames::langAttr))
        return false;
    if (context.element()->fastGetAttribute(langAttr) != sharingCandidate->fastGetAttribute(langAttr))
        return false;

    if (!m_elementAffectedByClassRules) {
        if (sharingCandidate->hasClass() && classNamesAffectedByRules(sharingCandidate->classNames()))
            return false;
    } else if (sharingCandidate->hasClass()) {
        // SVG elements require a (slow!) getAttribute comparision because "class" is an animatable attribute for SVG.
        if (context.element()->isSVGElement()) {
            if (context.element()->getAttribute(classAttr) != sharingCandidate->getAttribute(classAttr))
                return false;
        } else if (context.element()->classNames() != sharingCandidate->classNames()) {
            return false;
        }
    } else {
        return false;
    }

    if (context.element()->presentationAttributeStyle() != sharingCandidate->presentationAttributeStyle())
        return false;

    if (context.element()->hasTagName(progressTag)) {
        if (context.element()->shouldAppearIndeterminate() != sharingCandidate->shouldAppearIndeterminate())
            return false;
    }

    return true;
}

bool StyleResolver::canShareStyleWithElement(const ElementResolveContext& context, Element* element) const
{
    RenderStyle* style = element->renderStyle();
    if (!style)
        return false;
    if (style->unique())
        return false;
    if (style->hasUniquePseudoStyle())
        return false;
    if (element->tagQName() != context.element()->tagQName())
        return false;
    if (element->inlineStyle())
        return false;
    if (element->needsStyleRecalc())
        return false;
    if (element->isSVGElement() && toSVGElement(element)->animatedSMILStyleProperties())
        return false;
    if (element->isLink() != context.element()->isLink())
        return false;
    if (element->hovered() != context.element()->hovered())
        return false;
    if (element->active() != context.element()->active())
        return false;
    if (element->focused() != context.element()->focused())
        return false;
    if (element->shadowPseudoId() != context.element()->shadowPseudoId())
        return false;
    if (element == element->document()->cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(context, element))
        return false;
    if (element->additionalPresentationAttributeStyle() != context.element()->additionalPresentationAttributeStyle())
        return false;

    if (element->hasID() && m_features.idsInRules.contains(element->idForStyleResolution().impl()))
        return false;
    if (element->hasScopedHTMLStyleChild())
        return false;

    // FIXME: We should share style for option and optgroup whenever possible.
    // Before doing so, we need to resolve issues in HTMLSelectElement::recalcListItems
    // and RenderMenuList::setText. See also https://bugs.webkit.org/show_bug.cgi?id=88405
    if (element->hasTagName(optionTag) || isHTMLOptGroupElement(element))
        return false;

    bool isControl = element->isFormControlElement();

    if (isControl != context.element()->isFormControlElement())
        return false;

    if (isControl && !canShareStyleWithControl(context, element))
        return false;

    if (style->transitions() || style->animations())
        return false;

    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    if (element->hasTagName(iframeTag) || element->hasTagName(frameTag) || element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag) || element->hasTagName(canvasTag))
        return false;

    if (elementHasDirectionAuto(element))
        return false;

    if (element->isLink() && context.elementLinkState() != style->insideLink())
        return false;

    if (element->isUnresolvedCustomElement() != context.element()->isUnresolvedCustomElement())
        return false;

    // Deny sharing styles between WebVTT and non-WebVTT nodes.
    if (element->isWebVTTElement() != context.element()->isWebVTTElement())
        return false;

    if (element->isWebVTTElement() && context.element()->isWebVTTElement() && toWebVTTElement(element)->isPastNode() != toWebVTTElement(context.element())->isPastNode())
        return false;

    if (FullscreenController* fullscreen = FullscreenController::fromIfExists(context.document())) {
        if (element == fullscreen->webkitCurrentFullScreenElement() || context.element() == fullscreen->webkitCurrentFullScreenElement())
            return false;
    }

    return true;
}

inline Element* StyleResolver::findSiblingForStyleSharing(const ElementResolveContext& context, Node* node, unsigned& count) const
{
    for (; node; node = node->previousSibling()) {
        if (!node->isStyledElement())
            continue;
        if (canShareStyleWithElement(context, toElement(node)))
            break;
        if (count++ == cStyleSearchThreshold)
            return 0;
    }
    return toElement(node);
}

RenderStyle* StyleResolver::locateSharedStyle(const ElementResolveContext& context)
{
    if (!context.element() || !context.element()->isStyledElement())
        return 0;

    // If the element has inline style it is probably unique.
    if (context.element()->inlineStyle())
        return 0;
    if (context.element()->isSVGElement() && toSVGElement(context.element())->animatedSMILStyleProperties())
        return 0;
    // Ids stop style sharing if they show up in the stylesheets.
    if (context.element()->hasID() && m_features.idsInRules.contains(context.element()->idForStyleResolution().impl()))
        return 0;
    // Active and hovered elements always make a chain towards the document node
    // and no siblings or cousins will have the same state.
    if (context.element()->hovered())
        return 0;
    if (context.element()->active())
        return 0;
    // There is always only one focused element.
    if (context.element()->focused())
        return 0;
    if (parentElementPreventsSharing(context.element()->parentElement()))
        return 0;
    if (context.element()->hasScopedHTMLStyleChild())
        return 0;
    if (context.element() == context.document()->cssTarget())
        return 0;
    if (elementHasDirectionAuto(context.element()))
        return 0;
    if (context.element()->hasActiveAnimations())
        return 0;
    // When a dialog is first shown, its style is mutated to center it in the
    // viewport. So the styles can't be shared since the viewport position and
    // size may be different each time a dialog is opened.
    if (context.element()->hasTagName(dialogTag))
        return 0;

    // Cache whether context.element() is affected by any known class selectors.
    // FIXME: This should be an explicit out parameter, instead of a member variable.
    m_elementAffectedByClassRules = context.element() && context.element()->hasClass() && classNamesAffectedByRules(context.element()->classNames());

    // Check previous siblings and their cousins.
    unsigned count = 0;
    unsigned visitedNodeCount = 0;
    Element* shareElement = 0;
    Node* cousinList = context.element()->previousSibling();
    while (cousinList) {
        shareElement = findSiblingForStyleSharing(context, cousinList, count);
        if (shareElement)
            break;
        cousinList = locateCousinList(cousinList->parentElement(), visitedNodeCount);
    }

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return 0;

    // Can't share if sibling rules apply. This is checked at the end as it should rarely fail.
    if (styleSharingCandidateMatchesRuleSet(m_siblingRuleSet.get()))
        return 0;
    // Can't share if attribute rules apply.
    if (styleSharingCandidateMatchesRuleSet(m_uncommonAttributeRuleSet.get()))
        return 0;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (parentElementPreventsSharing(context.element()->parentElement()))
        return 0;
    return shareElement->renderStyle();
}

static void setStylesForPaginationMode(Pagination::Mode paginationMode, RenderStyle* style)
{
    if (paginationMode == Pagination::Unpaginated)
        return;

    switch (paginationMode) {
    case Pagination::LeftToRightPaginated:
        style->setColumnAxis(HorizontalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isLeftToRightDirection() ? NormalColumnProgression : ReverseColumnProgression);
        else
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? ReverseColumnProgression : NormalColumnProgression);
        break;
    case Pagination::RightToLeftPaginated:
        style->setColumnAxis(HorizontalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isLeftToRightDirection() ? ReverseColumnProgression : NormalColumnProgression);
        else
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? NormalColumnProgression : ReverseColumnProgression);
        break;
    case Pagination::TopToBottomPaginated:
        style->setColumnAxis(VerticalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? ReverseColumnProgression : NormalColumnProgression);
        else
            style->setColumnProgression(style->isLeftToRightDirection() ? NormalColumnProgression : ReverseColumnProgression);
        break;
    case Pagination::BottomToTopPaginated:
        style->setColumnAxis(VerticalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? NormalColumnProgression : ReverseColumnProgression);
        else
            style->setColumnProgression(style->isLeftToRightDirection() ? ReverseColumnProgression : NormalColumnProgression);
        break;
    case Pagination::Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

static void getFontAndGlyphOrientation(const RenderStyle* style, FontOrientation& fontOrientation, NonCJKGlyphOrientation& glyphOrientation)
{
    if (style->isHorizontalWritingMode()) {
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    }

    switch (style->textOrientation()) {
    case TextOrientationVerticalRight:
        fontOrientation = Vertical;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    case TextOrientationUpright:
        fontOrientation = Vertical;
        glyphOrientation = NonCJKGlyphOrientationUpright;
        return;
    case TextOrientationSideways:
        if (style->writingMode() == LeftToRightWritingMode) {
            // FIXME: This should map to sideways-left, which is not supported yet.
            fontOrientation = Vertical;
            glyphOrientation = NonCJKGlyphOrientationVerticalRight;
            return;
        }
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    case TextOrientationSidewaysRight:
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    default:
        ASSERT_NOT_REACHED();
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    }
}

PassRefPtr<RenderStyle> StyleResolver::styleForDocument(Document* document, CSSFontSelector* fontSelector)
{
    Frame* frame = document->frame();

    // HTML5 states that seamless iframes should replace default CSS values
    // with values inherited from the containing iframe element. However,
    // some values (such as the case of designMode = "on") still need to
    // be set by this "document style".
    RefPtr<RenderStyle> documentStyle = RenderStyle::create();
    bool seamlessWithParent = document->shouldDisplaySeamlesslyWithParent();
    if (seamlessWithParent) {
        RenderStyle* iframeStyle = document->seamlessParentIFrame()->renderStyle();
        if (iframeStyle)
            documentStyle->inheritFrom(iframeStyle);
    }

    // FIXME: It's not clear which values below we want to override in the seamless case!
    documentStyle->setDisplay(BLOCK);
    if (!seamlessWithParent) {
        documentStyle->setRTLOrdering(document->visuallyOrdered() ? VisualOrder : LogicalOrder);
        documentStyle->setZoom(frame && !document->printing() ? frame->pageZoomFactor() : 1);
        documentStyle->setLocale(document->contentLanguage());
    }
    // This overrides any -webkit-user-modify inherited from the parent iframe.
    documentStyle->setUserModify(document->inDesignMode() ? READ_WRITE : READ_ONLY);

    Element* docElement = document->documentElement();
    RenderObject* docElementRenderer = docElement ? docElement->renderer() : 0;
    if (docElementRenderer) {
        // Use the direction and writing-mode of the body to set the
        // viewport's direction and writing-mode unless the property is set on the document element.
        // If there is no body, then use the document element.
        RenderObject* bodyRenderer = document->body() ? document->body()->renderer() : 0;
        if (bodyRenderer && !document->writingModeSetOnDocumentElement())
            documentStyle->setWritingMode(bodyRenderer->style()->writingMode());
        else
            documentStyle->setWritingMode(docElementRenderer->style()->writingMode());
        if (bodyRenderer && !document->directionSetOnDocumentElement())
            documentStyle->setDirection(bodyRenderer->style()->direction());
        else
            documentStyle->setDirection(docElementRenderer->style()->direction());
    }

    if (frame) {
        if (FrameView* frameView = frame->view()) {
            const Pagination& pagination = frameView->pagination();
            if (pagination.mode != Pagination::Unpaginated) {
                setStylesForPaginationMode(pagination.mode, documentStyle.get());
                documentStyle->setColumnGap(pagination.gap);
                if (RenderView* view = document->renderView()) {
                    if (view->hasColumns())
                        view->updateColumnInfoFromStyle(documentStyle.get());
                }
            }
        }
    }

    // Seamless iframes want to inherit their font from their parent iframe, so early return before setting the font.
    if (seamlessWithParent)
        return documentStyle.release();

    FontDescription fontDescription;
    fontDescription.setScript(localeToScriptCodeForFontSelection(documentStyle->locale()));
    if (Settings* settings = document->settings()) {
        fontDescription.setUsePrinterFont(document->printing());
        const AtomicString& standardFont = settings->standardFontFamily(fontDescription.script());
        if (!standardFont.isEmpty()) {
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.firstFamily().setFamily(standardFont);
            fontDescription.firstFamily().appendFamily(0);
        }
        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        int size = FontSize::fontSizeForKeyword(document, CSSValueMedium, false);
        fontDescription.setSpecifiedSize(size);
        bool useSVGZoomRules = document->isSVGDocument();
        fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(document, documentStyle.get(), fontDescription.isAbsoluteSize(), size, useSVGZoomRules));
    } else
        fontDescription.setUsePrinterFont(document->printing());

    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(documentStyle.get(), fontOrientation, glyphOrientation);
    fontDescription.setOrientation(fontOrientation);
    fontDescription.setNonCJKGlyphOrientation(glyphOrientation);

    documentStyle->setFontDescription(fontDescription);
    documentStyle->font().update(fontSelector);

    return documentStyle.release();
}

static inline bool isAtShadowBoundary(const Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

PassRefPtr<RenderStyle> StyleResolver::styleForElement(Element* element, RenderStyle* defaultParent, StyleSharingBehavior sharingBehavior,
    RuleMatchingBehavior matchingBehavior, RenderRegion* regionForStyling)
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !element->document()->haveStylesheetsLoaded() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        element->document()->setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    StyleResolverState& state = m_state;
    state.initForStyleResolve(document(), element, defaultParent, regionForStyling);
    if (sharingBehavior == AllowStyleSharing && !state.distributedToInsertionPoint() && state.parentStyle()) {
        RenderStyle* sharedStyle = locateSharedStyle(state.elementContext());
        if (sharedStyle) {
            state.clear();
            return sharedStyle;
        }
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
    CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(element, needsCollection);
    if (needsCollection) {
        collectFeatures();
        m_inspectorCSSOMWrappers.reset();
    }

    {
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style(), m_inspectorCSSOMWrappers);
        collector.setRegionForStyling(regionForStyling);

        if (matchingBehavior == MatchOnlyUserAgentRules)
            matchUARules(collector);
        else
            matchAllRules(collector, m_matchAuthorAndUserStyles, matchingBehavior != MatchAllRulesExcludingSMIL);

        applyMatchedProperties(collector.matchedResult(), element);
    }
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(state.style(), state.parentStyle(), element);

    state.clear(); // Clear out for the next resolve.

    document()->didAccessStyleResolver();

    // FIXME: Shouldn't this be on RenderBody::styleDidChange?
    if (element->hasTagName(bodyTag))
        document()->textLinkColors().setTextColor(state.style()->visitedDependentColor(CSSPropertyColor));

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForKeyframe(const RenderStyle* elementStyle, const StyleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    MatchResult result;
    if (keyframe->properties())
        result.addMatchedProperties(keyframe->properties());

    StyleResolverState& state = m_state;
    ASSERT(!state.style());

    // Create the style
    state.setStyle(RenderStyle::clone(elementStyle));
    state.setLineHeightValue(0);

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    bool inheritedOnly = false;
    if (keyframe->properties()) {
        // FIXME: Can't keyframes contain variables?
        applyMatchedProperties<AnimationProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
        applyMatchedProperties<HighPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, state.lineHeightValue());

    // Now do rest of the properties.
    if (keyframe->properties())
        applyMatchedProperties<LowPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied by one of the non-essential font props,
    // go ahead and update it a second time.
    updateFont();

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());

    // Add all the animating properties to the keyframe.
    if (const StylePropertySet* styleDeclaration = keyframe->properties()) {
        unsigned propertyCount = styleDeclaration->propertyCount();
        for (unsigned i = 0; i < propertyCount; ++i) {
            CSSPropertyID property = styleDeclaration->propertyAt(i).id();
            // Timing-function within keyframes is special, because it is not animated; it just
            // describes the timing function between this keyframe and the next.
            if (property != CSSPropertyWebkitAnimationTimingFunction)
                keyframeValue.addProperty(property);
        }
    }

    document()->didAccessStyleResolver();

    return state.takeStyle();
}

void StyleResolver::keyframeStylesForAnimation(Element* e, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();

    // Get the keyframesRule for this name
    if (!e || list.animationName().isEmpty())
        return;

    m_keyframesRuleMap.checkConsistency();

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(list.animationName().impl());
    if (it == m_keyframesRuleMap.end())
        return;

    const StyleRuleKeyframes* keyframesRule = it->value.get();

    // Construct and populate the style for each keyframe
    const Vector<RefPtr<StyleKeyframe> >& keyframes = keyframesRule->keyframes();
    for (unsigned i = 0; i < keyframes.size(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        m_state.initForStyleResolve(document(), e);

        const StyleKeyframe* keyframe = keyframes[i].get();

        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, keyframe, keyframeValue));

        // Add this keyframe style to all the indicated key times
        Vector<float> keys;
        keyframe->getKeys(keys);
        for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            keyframeValue.setKey(keys[keyIndex]);
            list.insert(keyframeValue);
        }
    }

    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe)
    int initialListSize = list.size();
    if (initialListSize > 0 && list[0].key()) {
        static StyleKeyframe* zeroPercentKeyframe;
        if (!zeroPercentKeyframe) {
            zeroPercentKeyframe = StyleKeyframe::create().leakRef();
            zeroPercentKeyframe->setKeyText("0%");
        }
        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, zeroPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe)
    if (initialListSize > 0 && (list[list.size() - 1].key() != 1)) {
        static StyleKeyframe* hundredPercentKeyframe;
        if (!hundredPercentKeyframe) {
            hundredPercentKeyframe = StyleKeyframe::create().leakRef();
            hundredPercentKeyframe->setKeyText("100%");
        }
        KeyframeValue keyframeValue(1, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, hundredPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element* e, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle)
{
    ASSERT(parentStyle);
    if (!e)
        return 0;

    StyleResolverState& state = m_state;

    state.initForStyleResolve(document(), e, parentStyle);

    if (pseudoStyleRequest.allowsInheritance(state.parentStyle())) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    // Since we don't use pseudo-elements in any of our quirk/print
    // user agent rules, don't waste time walking those rules.

    {
        // Check UA, user and author rules.
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style(), m_inspectorCSSOMWrappers);
        collector.setPseudoStyleRequest(pseudoStyleRequest);

        matchUARules(collector);
        if (m_matchAuthorAndUserStyles) {
            matchUserRules(collector, false);
            matchAuthorRules(collector, false);
        }

        if (collector.matchedResult().matchedProperties.isEmpty())
            return 0;

        state.style()->setStyleType(pseudoStyleRequest.pseudoId);

        applyMatchedProperties(collector.matchedResult(), e);
    }
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(state.style(), state.parentStyle(), 0);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());

    document()->didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    m_state.initForStyleResolve(document(), document()->documentElement()); // m_rootElementStyle will be set to the document style.

    m_state.setStyle(RenderStyle::create());
    m_state.style()->inheritFrom(m_state.rootElementStyle());

    PageRuleCollector collector(m_state, pageIndex);

    collector.matchPageRules(CSSDefaultStyleSheets::defaultPrintStyle);
    collector.matchPageRules(m_ruleSets.userStyle());

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        scopedResolver->matchPageRules(collector);

    m_state.setLineHeightValue(0);
    bool inheritedOnly = false;

    MatchResult& result = collector.matchedResult();
    applyMatchedProperties<VariableDefinitions>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (m_state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, m_state.lineHeightValue());

    applyMatchedProperties<LowPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(m_state.style(), m_state.elementStyleResources());

    document()->didAccessStyleResolver();

    // Now return the style.
    return m_state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::defaultStyleForElement()
{
    m_state.setStyle(RenderStyle::create());
    // Make sure our fonts are initialized if we don't inherit them from our parent style.
    if (Settings* settings = documentSettings()) {
        initializeFontStyle(settings);
        m_state.style()->font().update(fontSelector());
    } else
        m_state.style()->font().update(0);

    return m_state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForText(Text* textNode)
{
    ASSERT(textNode);

    NodeRenderingContext context(textNode);
    Node* parentNode = context.parentNodeForRenderingAndStyle();
    return context.resetStyleInheritance() || !parentNode || !parentNode->renderStyle() ?
        defaultStyleForElement() : parentNode->renderStyle();
}

static void addIntrinsicMargins(RenderStyle* style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2 * style->effectiveZoom();

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "quirk" to decide the margin wasn't set is kind of lame.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(intrinsicMargin, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(intrinsicMargin, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(intrinsicMargin, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(intrinsicMargin, Fixed));
    }
}

static EDisplay equivalentBlockDisplay(EDisplay display, bool isFloating, bool strictParsing)
{
    switch (display) {
    case BLOCK:
    case TABLE:
    case BOX:
    case FLEX:
    case GRID:
    case LAZY_BLOCK:
        return display;

    case LIST_ITEM:
        // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk, but only in quirks mode.
        if (!strictParsing && isFloating)
            return BLOCK;
        return display;
    case INLINE_TABLE:
        return TABLE;
    case INLINE_BOX:
        return BOX;
    case INLINE_FLEX:
        return FLEX;
    case INLINE_GRID:
        return GRID;

    case INLINE:
    case RUN_IN:
    case COMPACT:
    case INLINE_BLOCK:
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
    case TABLE_ROW:
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
    case TABLE_CELL:
    case TABLE_CAPTION:
        return BLOCK;
    case NONE:
        ASSERT_NOT_REACHED();
        return NONE;
    }
    ASSERT_NOT_REACHED();
    return BLOCK;
}

// CSS requires text-decoration to be reset at each DOM element for tables,
// inline blocks, inline tables, run-ins, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements.
static bool doesNotInheritTextDecoration(const RenderStyle* style, const Element* e)
{
    return style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN
        || style->display() == INLINE_BLOCK || style->display() == INLINE_BOX || isAtShadowBoundary(e)
        || style->isFloating() || style->hasOutOfFlowPosition();
}

// FIXME: This helper is only needed because pseudoStyleForElement passes a null
// element to adjustRenderStyle, so we can't just use element->isInTopLayer().
static bool isInTopLayer(const Element* element, const RenderStyle* style)
{
    return (element && element->isInTopLayer()) || (style && style->styleType() == BACKDROP);
}

static bool isDisplayFlexibleBox(EDisplay display)
{
    return display == FLEX || display == INLINE_FLEX;
}

static bool isDisplayGridBox(EDisplay display)
{
    return display == GRID || display == INLINE_GRID;
}

void StyleResolver::adjustRenderStyle(RenderStyle* style, RenderStyle* parentStyle, Element *e)
{
    ASSERT(parentStyle);

    // Cache our original display.
    style->setOriginalDisplay(style->display());

    if (style->display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s. In quirks mode we force
        // these tags to retain their display types.
        if (document()->inQuirksMode() && e) {
            if (e->hasTagName(tdTag)) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(NoFloat);
            } else if (isHTMLTableElement(e))
                style->setDisplay(style->isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        if (e && (e->hasTagName(tdTag) || e->hasTagName(thTag))) {
            if (style->whiteSpace() == KHTML_NOWRAP) {
                // Figure out if we are really nowrapping or if we should just
                // use normal instead. If the width of the cell is fixed, then
                // we don't actually use NOWRAP.
                if (style->width().isFixed())
                    style->setWhiteSpace(NORMAL);
                else
                    style->setWhiteSpace(NOWRAP);
            }
        }

        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (e && isHTMLTableElement(e) && (style->textAlign() == WEBKIT_LEFT || style->textAlign() == WEBKIT_CENTER || style->textAlign() == WEBKIT_RIGHT))
            style->setTextAlign(TASTART);

        // Frames and framesets never honor position:relative or position:absolute. This is necessary to
        // fix a crash where a site tries to position these objects. They also never honor display.
        if (e && (e->hasTagName(frameTag) || e->hasTagName(framesetTag))) {
            style->setPosition(StaticPosition);
            style->setDisplay(BLOCK);
        }

        // Ruby text does not support float or position. This might change with evolution of the specification.
        if (e && e->hasTagName(rtTag)) {
            style->setPosition(StaticPosition);
            style->setFloating(NoFloat);
        }

        // FIXME: We shouldn't be overriding start/-webkit-auto like this. Do it in html.css instead.
        // Table headers with a text-align of -webkit-auto will change the text-align to center.
        if (e && e->hasTagName(thTag) && style->textAlign() == TASTART)
            style->setTextAlign(CENTER);

        if (e && e->hasTagName(legendTag))
            style->setDisplay(BLOCK);

        // Per the spec, position 'static' and 'relative' in the top layer compute to 'absolute'.
        if (isInTopLayer(e, style) && (style->position() == StaticPosition || style->position() == RelativePosition))
            style->setPosition(AbsolutePosition);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style->hasOutOfFlowPosition() || style->isFloating() || (e && e->document()->documentElement() == e))
            style->setDisplay(equivalentBlockDisplay(style->display(), style->isFloating(), !document()->inQuirksMode()));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style->display() == INLINE && style->styleType() == NOPSEUDO && style->writingMode() != parentStyle->writingMode())
            style->setDisplay(INLINE_BLOCK);

        // After performing the display mutation, check table rows. We do not honor position:relative or position:sticky on
        // table rows or cells. This has been established for position:relative in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW_GROUP
            || style->display() == TABLE_FOOTER_GROUP || style->display() == TABLE_ROW)
            && style->hasInFlowPosition())
            style->setPosition(StaticPosition);

        // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
        // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
        if (style->display() == TABLE_COLUMN || style->display() == TABLE_COLUMN_GROUP || style->display() == TABLE_FOOTER_GROUP
            || style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW || style->display() == TABLE_ROW_GROUP
            || style->display() == TABLE_CELL)
            style->setWritingMode(parentStyle->writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style->writingMode() != TopToBottomWritingMode && (style->display() == BOX || style->display() == INLINE_BOX))
            style->setWritingMode(TopToBottomWritingMode);

        if (isDisplayFlexibleBox(parentStyle->display()) || isDisplayGridBox(parentStyle->display())) {
            style->setFloating(NoFloat);
            style->setDisplay(equivalentBlockDisplay(style->display(), style->isFloating(), !document()->inQuirksMode()));
        }
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style->position() == StaticPosition && !isDisplayFlexibleBox(parentStyle->display()))
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e)
        || style->opacity() < 1.0f
        || style->hasTransformRelatedProperty()
        || style->hasMask()
        || style->clipPath()
        || style->boxReflect()
        || style->hasFilter()
        || style->hasBlendMode()
        || style->position() == StickyPosition
        || (style->position() == FixedPosition && e && e->document()->page() && e->document()->page()->settings()->fixedPositionCreatesStackingContext())
        || isInTopLayer(e, style)
        ))
        style->setZIndex(0);

    // Textarea considers overflow visible as auto.
    if (e && isHTMLTextAreaElement(e)) {
        style->setOverflowX(style->overflowX() == OVISIBLE ? OAUTO : style->overflowX());
        style->setOverflowY(style->overflowY() == OVISIBLE ? OAUTO : style->overflowY());
    }

    // For now, <marquee> requires an overflow clip to work properly.
    if (e && e->hasTagName(marqueeTag)) {
        style->setOverflowX(OHIDDEN);
        style->setOverflowY(OHIDDEN);
    }

    if (doesNotInheritTextDecoration(style, e))
        style->setTextDecorationsInEffect(style->textDecoration());
    else
        style->addToTextDecorationsInEffect(style->textDecoration());

    // If either overflow value is not visible, change to auto.
    if (style->overflowX() == OVISIBLE && style->overflowY() != OVISIBLE) {
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        style->setOverflowX(OAUTO);
    } else if (style->overflowY() == OVISIBLE && style->overflowX() != OVISIBLE)
        style->setOverflowY(OAUTO);

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // StyleResolver::styleForDocument().
    if ((style->overflowY() == OPAGEDX || style->overflowY() == OPAGEDY) && !(e && (isHTMLHtmlElement(e) || e->hasTagName(bodyTag))))
        setStylesForPaginationMode(WebCore::paginationModeForRenderStyle(style), style);

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style->display() == TABLE || style->display() == INLINE_TABLE
        || style->display() == TABLE_ROW_GROUP || style->display() == TABLE_ROW) {
        if (style->overflowX() != OVISIBLE && style->overflowX() != OHIDDEN)
            style->setOverflowX(OVISIBLE);
        if (style->overflowY() != OVISIBLE && style->overflowY() != OHIDDEN)
            style->setOverflowY(OVISIBLE);
    }

    // Menulists should have visible overflow
    if (style->appearance() == MenulistPart) {
        style->setOverflowX(OVISIBLE);
        style->setOverflowY(OVISIBLE);
    }

    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();
    style->adjustMaskLayers();

    // Do the same for animations and transitions.
    style->adjustAnimations();
    style->adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (e && e->isFormControlElement() && style->fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!e->hasTagName(inputTag) || !toHTMLInputElement(e)->isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance())
        RenderTheme::defaultTheme()->adjustStyle(style, e, m_state.cachedUAStyle());

    // If we have first-letter pseudo style, do not share this style.
    if (style->hasPseudoStyle(FIRST_LETTER))
        style->setUnique();

    // FIXME: when dropping the -webkit prefix on transform-style, we should also have opacity < 1 cause flattening.
    if (style->preserves3D() && (style->overflowX() != OVISIBLE
        || style->overflowY() != OVISIBLE
        || style->hasFilter()))
        style->setTransformStyle3D(TransformStyle3DFlat);

    // Seamless iframes behave like blocks. Map their display to inline-block when marked inline.
    if (e && e->hasTagName(iframeTag) && style->display() == INLINE && static_cast<HTMLIFrameElement*>(e)->shouldDisplaySeamlessly())
        style->setDisplay(INLINE_BLOCK);

    adjustGridItemPosition(style);

    if (e && e->isSVGElement()) {
        // Spec: http://www.w3.org/TR/SVG/masking.html#OverflowProperty
        if (style->overflowY() == OSCROLL)
            style->setOverflowY(OHIDDEN);
        else if (style->overflowY() == OAUTO)
            style->setOverflowY(OVISIBLE);

        if (style->overflowX() == OSCROLL)
            style->setOverflowX(OHIDDEN);
        else if (style->overflowX() == OAUTO)
            style->setOverflowX(OVISIBLE);

        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(e->hasTagName(SVGNames::svgTag) && e->parentNode() && !e->parentNode()->isSVGElement()))
            style->setPosition(RenderStyle::initialPosition());

        // RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
        // not be scaled again.
        if (e->hasTagName(SVGNames::foreignObjectTag))
            style->setEffectiveZoom(RenderStyle::initialZoom());
    }
}

void StyleResolver::adjustGridItemPosition(RenderStyle* style) const
{
    // If opposing grid-placement properties both specify a grid span, they both compute to ‘auto’.
    if (style->gridColumnStart().isSpan() && style->gridColumnEnd().isSpan()) {
        style->setGridColumnStart(GridPosition());
        style->setGridColumnEnd(GridPosition());
    }

    if (style->gridRowStart().isSpan() && style->gridRowEnd().isSpan()) {
        style->setGridRowStart(GridPosition());
        style->setGridRowEnd(GridPosition());
    }
}

bool StyleResolver::checkRegionStyle(Element* regionElement)
{
    // FIXME (BUG 72472): We don't add @-webkit-region rules of scoped style sheets for the moment,
    // so all region rules are global by default. Verify whether that can stand or needs changing.

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        if (scopedResolver->checkRegionStyle(regionElement))
            return true;

    if (m_ruleSets.userStyle()) {
        unsigned rulesSize = m_ruleSets.userStyle()->m_regionSelectorsAndRuleSets.size();
        for (unsigned i = 0; i < rulesSize; ++i) {
            ASSERT(m_ruleSets.userStyle()->m_regionSelectorsAndRuleSets.at(i).ruleSet.get());
            if (checkRegionSelector(m_ruleSets.userStyle()->m_regionSelectorsAndRuleSets.at(i).selector, regionElement))
                return true;
        }
    }

    return false;
}

static void checkForOrientationChange(RenderStyle* style)
{
    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(style, fontOrientation, glyphOrientation);

    const FontDescription& fontDescription = style->fontDescription();
    if (fontDescription.orientation() == fontOrientation && fontDescription.nonCJKGlyphOrientation() == glyphOrientation)
        return;

    FontDescription newFontDescription(fontDescription);
    newFontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    newFontDescription.setOrientation(fontOrientation);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::updateFont()
{
    if (!m_state.fontDirty())
        return;

    RenderStyle* style = m_state.style();
    checkForGenericFamilyChange(style, m_state.parentStyle());
    checkForZoomChange(style, m_state.parentStyle());
    checkForOrientationChange(style);
    style->font().update(m_fontSelector);
    m_state.setFontDirty(false);
}

PassRefPtr<CSSRuleList> StyleResolver::styleRulesForElement(Element* e, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(e, NOPSEUDO, rulesToInclude);
}

PassRefPtr<CSSRuleList> StyleResolver::pseudoStyleRulesForElement(Element* e, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!e || !e->document()->haveStylesheetsLoaded())
        return 0;

    m_state.initForStyleResolve(document(), e, 0);

    ElementRuleCollector collector(m_state.elementContext(), m_selectorFilter, m_state.style(), m_inspectorCSSOMWrappers);
    collector.setMode(SelectorChecker::CollectingRules);
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));

    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        matchUARules(collector);

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            matchUserRules(collector, rulesToInclude & EmptyCSSRules);
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules)) {
        collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));

        // Check the rules in author sheets.
        matchAuthorRules(collector, rulesToInclude & EmptyCSSRules);
    }

    return collector.matchedRuleList();
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyAnimatedProperties(const Element* target)
{
    ASSERT(pass != VariableDefinitions);
    ASSERT(pass != AnimationProperties);
    if (!target->hasActiveAnimations())
        return;

    Vector<Animation*>* animations = target->activeAnimations();

    for (size_t i = 0; i < animations->size(); ++i) {
        RefPtr<Animation> animation = animations->at(i);
        const AnimationEffect::CompositableValueMap* compositableValues = animation->compositableValues();
        for (AnimationEffect::CompositableValueMap::const_iterator iter = compositableValues->begin(); iter != compositableValues->end(); ++iter) {
            CSSPropertyID property = iter->key;
            if (!isPropertyForPass<pass>(property))
                continue;
            RefPtr<CSSValue> value = iter->value->compositeOnto(AnimatableValue::neutralValue())->toCSSValue();
            if (pass == HighPriorityProperties && property == CSSPropertyLineHeight)
                m_state.setLineHeightValue(value.get());
            else
                applyProperty(property, value.get());
        }
    }
}

static inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyOutlineColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    default:
        break;
    }

    return false;
}

// http://dev.w3.org/csswg/css3-regions/#the-at-region-style-rule
// FIXME: add incremental support for other region styling properties.
static inline bool isValidRegionStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyColor:
        return true;
    default:
        break;
    }

    return false;
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
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    default:
        break;
    }
    return false;
}

template <StyleResolver::StyleApplicationPass pass>
bool StyleResolver::isPropertyForPass(CSSPropertyID property)
{
    COMPILE_ASSERT(CSSPropertyVariable < firstCSSProperty, CSS_variable_is_before_first_property);
    const CSSPropertyID firstAnimationProperty = CSSPropertyWebkitAnimation;
    const CSSPropertyID lastAnimationProperty = CSSPropertyTransitionTimingFunction;
    COMPILE_ASSERT(firstCSSProperty == firstAnimationProperty, CSS_first_animation_property_should_be_first_property);
    const CSSPropertyID firstHighPriorityProperty = CSSPropertyColor;
    const CSSPropertyID lastHighPriorityProperty = CSSPropertyLineHeight;
    COMPILE_ASSERT(lastAnimationProperty + 1 == firstHighPriorityProperty, CSS_color_is_first_high_priority_property);
    COMPILE_ASSERT(CSSPropertyLineHeight == firstHighPriorityProperty + 18, CSS_line_height_is_end_of_high_prioity_property_range);
    COMPILE_ASSERT(CSSPropertyZoom == lastHighPriorityProperty - 1, CSS_zoom_is_before_line_height);
    switch (pass) {
    case VariableDefinitions:
        return property == CSSPropertyVariable;
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
void StyleResolver::applyProperties(const StylePropertySet* properties, StyleRule* rule, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType)
{
    ASSERT((propertyWhitelistType != PropertyWhitelistRegion) || m_state.regionForStyling());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willProcessRule(document(), rule, this);

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

        if (propertyWhitelistType == PropertyWhitelistRegion && !isValidRegionStyleProperty(property))
            continue;
        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(property))
            continue;
        if (!isPropertyForPass<pass>(property))
            continue;
        if (pass == HighPriorityProperties && property == CSSPropertyLineHeight)
            m_state.setLineHeightValue(current.value());
        else
            applyProperty(current.id(), current.value());
    }
    InspectorInstrumentation::didProcessRule(cookie);
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    StyleResolverState& state = m_state;
    if (state.style()->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
            unsigned linkMatchType = matchedProperties.linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            state.setApplyPropertyToRegularStyle(linkMatchType & SelectorChecker::MatchLink);
            state.setApplyPropertyToVisitedLinkStyle(linkMatchType & SelectorChecker::MatchVisited);

            applyProperties<pass>(matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
        }
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        applyProperties<pass>(matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
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

void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, const Element* element)
{
    ASSERT(element);
    StyleResolverState& state = m_state;
    unsigned cacheHash = matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const CachedMatchedProperties* cachedMatchedProperties = 0;
    if (cacheHash && (cachedMatchedProperties = m_matchedPropertiesCache.find(cacheHash, m_state, matchResult))) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cachedMatchedProperties->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cachedMatchedProperties->parentRenderStyle.get()) && !isAtShadowBoundary(element)) {
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

    // First apply all variable definitions, as they may be used during application of later properties.
    applyMatchedProperties<VariableDefinitions>(matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Apply animation properties in order to apply animation results and trigger transitions below.
    applyMatchedProperties<AnimationProperties>(matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
    // FIXME: animations should be triggered here

    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    state.setLineHeightValue(0);
    applyMatchedProperties<HighPriorityProperties>(matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    // Animation contributions are processed here because CSS Animations are overridable by user !important rules.
    if (RuntimeEnabledFeatures::webAnimationsEnabled())
        applyAnimatedProperties<HighPriorityProperties>(element);
    applyMatchedProperties<HighPriorityProperties>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->effectiveZoom() != state.style()->effectiveZoom()) {
        state.setFontDirty(true);
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, state.lineHeightValue());

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->fontDescription() != state.style()->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyMatchedProperties<LowPriorityProperties>(matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Cache the UA properties to pass them to RenderTheme in adjustRenderStyle.
    state.cacheUserAgentBorderAndBackground();

    // Now do the author and user normal priority properties and all the !important properties.
    applyMatchedProperties<LowPriorityProperties>(matchResult, false, matchResult.ranges.lastUARule + 1, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    if (RuntimeEnabledFeatures::webAnimationsEnabled())
        applyAnimatedProperties<LowPriorityProperties>(element);
    applyMatchedProperties<LowPriorityProperties>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());

    ASSERT(!state.fontDirty());

    if (cachedMatchedProperties || !cacheHash)
        return;
    if (!MatchedPropertiesCache::isCacheable(state.element(), state.style(), state.parentStyle()))
        return;
    m_matchedPropertiesCache.add(state.style(), state.parentStyle(), cacheHash, matchResult);
}

void StyleResolver::applyPropertyToStyle(CSSPropertyID id, CSSValue* value, RenderStyle* style)
{
    m_state.initForStyleResolve(document(), 0, style);
    m_state.setStyle(style);
    applyPropertyToCurrentStyle(id, value);
}

void StyleResolver::applyPropertyToCurrentStyle(CSSPropertyID id, CSSValue* value)
{
    if (value)
        applyProperty(id, value);
}

static bool hasVariableReference(CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        return primitiveValue->hasVariableReference();
    }

    if (value->isCalculationValue())
        return static_cast<CSSCalcValue*>(value)->hasVariableReference();

    if (value->isReflectValue()) {
        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        CSSPrimitiveValue* direction = reflectValue->direction();
        CSSPrimitiveValue* offset = reflectValue->offset();
        CSSValue* mask = reflectValue->mask();
        return (direction && hasVariableReference(direction)) || (offset && hasVariableReference(offset)) || (mask && hasVariableReference(mask));
    }

    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        if (hasVariableReference(i.value()))
            return true;
    }

    return false;
}

void StyleResolver::resolveVariables(CSSPropertyID id, CSSValue* value, Vector<std::pair<CSSPropertyID, String> >& knownExpressions)
{
    std::pair<CSSPropertyID, String> expression(id, value->serializeResolvingVariables(*m_state.style()->variables()));

    if (knownExpressions.contains(expression))
        return; // cycle detected.

    knownExpressions.append(expression);

    // FIXME: It would be faster not to re-parse from strings, but for now CSS property validation lives inside the parser so we do it there.
    RefPtr<MutableStylePropertySet> resultSet = MutableStylePropertySet::create();
    if (!CSSParser::parseValue(resultSet.get(), id, expression.second, false, document()))
        return; // expression failed to parse.

    for (unsigned i = 0; i < resultSet->propertyCount(); i++) {
        StylePropertySet::PropertyReference property = resultSet->propertyAt(i);
        if (property.id() != CSSPropertyVariable && hasVariableReference(property.value()))
            resolveVariables(property.id(), property.value(), knownExpressions);
        else
            applyProperty(property.id(), property.value());
    }
}

void StyleResolver::applyProperty(CSSPropertyID id, CSSValue* value)
{
    if (id != CSSPropertyVariable && hasVariableReference(value)) {
        Vector<std::pair<CSSPropertyID, String> > knownExpressions;
        resolveVariables(id, value, knownExpressions);
        return;
    }

    // CSS variables don't resolve shorthands at parsing time, so this should be *after* handling variables.
    ASSERT_WITH_MESSAGE(!isExpandedShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    StyleResolverState& state = m_state;
    bool isInherit = state.parentNode() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentNode() && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    ASSERT(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;
    if (primitiveValue && primitiveValue->getValueID() == CSSValueCurrentcolor)
        state.style()->setHasCurrentColor();

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();

    if (id == CSSPropertyVariable) {
        ASSERT_WITH_SECURITY_IMPLICATION(value->isVariableValue());
        CSSVariableValue* variable = static_cast<CSSVariableValue*>(value);
        ASSERT(!variable->name().isEmpty());
        ASSERT(!variable->value().isEmpty());
        state.style()->setVariable(variable->name(), variable->value());
        return;
    }

    // Check lookup table for implementations and use when available.
    const PropertyHandler& handler = m_styleBuilder.propertyHandler(id);
    if (handler.isValid()) {
        if (isInherit)
            handler.applyInheritValue(id, this, state);
        else if (isInitial)
            handler.applyInitialValue(id, this, state);
        else
            handler.applyValue(id, this, state, value);
        return;
    }

    // Use the new StyleBuilder.
    if (StyleBuilder::applyProperty(id, this, state, value, isInitial, isInherit))
        return;

    // Fall back to the old switch statement, which is now in StyleBuilderCustom.cpp
    StyleBuilder::oldApplyProperty(id, this, state, value, isInitial, isInherit);
}

void StyleResolver::checkForZoomChange(RenderStyle* style, RenderStyle* parentStyle)
{
    if (style->effectiveZoom() == parentStyle->effectiveZoom())
        return;

    const FontDescription& childFont = style->fontDescription();
    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void StyleResolver::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const FontDescription& childFont = style->fontDescription();

    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFont = parentStyle->fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;

    // For now, lump all families but monospace together.
    if (childFont.genericFamily() != FontDescription::MonospaceFamily
        && parentFont.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (childFont.keywordSize())
        size = FontSize::fontSizeForKeyword(document(), CSSValueXxSmall + childFont.keywordSize() - 1, childFont.useFixedDefaultSize());
    else {
        Settings* settings = documentSettings();
        float fixedScaleFactor = (settings && settings->defaultFixedFontSize() && settings->defaultFontSize())
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFont.useFixedDefaultSize() ?
            childFont.specifiedSize() / fixedScaleFactor :
            childFont.specifiedSize() * fixedScaleFactor;
    }

    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, size);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::initializeFontStyle(Settings* settings)
{
    FontDescription fontDescription;
    fontDescription.setGenericFamily(FontDescription::StandardFamily);
    fontDescription.setUsePrinterFont(document()->printing());
    const AtomicString& standardFontFamily = documentSettings()->standardFontFamily();
    if (!standardFontFamily.isEmpty()) {
        fontDescription.firstFamily().setFamily(standardFontFamily);
        fontDescription.firstFamily().appendFamily(0);
    }
    fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
    setFontSize(fontDescription, FontSize::fontSizeForKeyword(document(), CSSValueMedium, false));
    m_state.style()->setLineHeight(RenderStyle::initialLineHeight());
    m_state.setLineHeightValue(0);
    m_state.setFontDescription(fontDescription);
}

void StyleResolver::setFontSize(FontDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(document(), m_state.style(), fontDescription.isAbsoluteSize(), size, m_state.useSVGZoomRules()));
}

void StyleResolver::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(adoptPtr(new MediaQueryResult(*expr, result)));
}

bool StyleResolver::affectedByViewportChange() const
{
    unsigned s = m_viewportDependentMediaQueryResults.size();
    for (unsigned i = 0; i < s; i++) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

void StyleResolver::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_ruleSets, "ruleSets");
    info.addMember(m_matchedPropertiesCache, "matchedPropertiesCache");

    info.addMember(m_medium, "medium");
    info.addMember(m_rootDefaultStyle, "rootDefaultStyle");
    info.addMember(m_document, "document");

    info.addMember(m_fontSelector, "fontSelector");
    info.addMember(m_viewportDependentMediaQueryResults, "viewportDependentMediaQueryResults");
    info.ignoreMember(m_styleBuilder);
    info.addMember(m_inspectorCSSOMWrappers);

    info.addMember(m_styleTree, "scopedStyleTree");
    info.addMember(m_state, "state");

    // FIXME: move this to a place where it would be called only once?
    info.addMember(CSSDefaultStyleSheets::defaultStyle, "defaultStyle");
    info.addMember(CSSDefaultStyleSheets::defaultQuirksStyle, "defaultQuirksStyle");
    info.addMember(CSSDefaultStyleSheets::defaultPrintStyle, "defaultPrintStyle");
    info.addMember(CSSDefaultStyleSheets::defaultViewSourceStyle, "defaultViewSourceStyle");
}

} // namespace WebCore
