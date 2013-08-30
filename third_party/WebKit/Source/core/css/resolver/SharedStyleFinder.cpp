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
 */

#include "config.h"
#include "core/css/resolver/SharedStyleFinder.h"

#include "HTMLNames.h"
#include "XMLNames.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/Node.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/SpaceSplitString.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/track/WebVTTElement.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/svg/SVGElement.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

using namespace HTMLNames;

static const unsigned cStyleSearchThreshold = 10;
static const unsigned cStyleSearchLevelThreshold = 10;

static inline bool parentElementPreventsSharing(const Element* parentElement)
{
    if (!parentElement)
        return false;
    return parentElement->hasFlagsSetDuringStylingOfChildren();
}

Node* SharedStyleFinder::locateCousinList(Element* parent, unsigned& visitedNodeCount) const
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
    if (isShadowHost(parent) && parent->shadow()->containsActiveStyles())
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


bool SharedStyleFinder::canShareStyleWithControl(const ElementResolveContext& context, Element* element) const
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

    if (context.document().containsValidityStyleRules()) {
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

bool SharedStyleFinder::classNamesAffectedByRules(const SpaceSplitString& classNames) const
{
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (m_features.classesInRules.contains(classNames[i].impl()))
            return true;
    }
    return false;
}

static inline bool elementHasDirectionAuto(Element* element)
{
    // FIXME: This line is surprisingly hot, we may wish to inline hasDirectionAuto into StyleResolver.
    return element->isHTMLElement() && toHTMLElement(element)->hasDirectionAuto();
}

bool SharedStyleFinder::sharingCandidateHasIdenticalStyleAffectingAttributes(const ElementResolveContext& context, Element* sharingCandidate) const
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

bool SharedStyleFinder::canShareStyleWithElement(const ElementResolveContext& context, Element* element) const
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
    if (element == element->document().cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(context, element))
        return false;
    if (element->additionalPresentationAttributeStyle() != context.element()->additionalPresentationAttributeStyle())
        return false;

    if (element->hasID() && m_features.idsInRules.contains(element->idForStyleResolution().impl()))
        return false;
    if (element->hasScopedHTMLStyleChild())
        return false;
    if (isShadowHost(element) && element->shadow()->containsActiveStyles())
        return 0;

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

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(&context.document())) {
        if (element == fullscreen->webkitCurrentFullScreenElement() || context.element() == fullscreen->webkitCurrentFullScreenElement())
            return false;
    }

    return true;
}

inline Element* SharedStyleFinder::findSiblingForStyleSharing(const ElementResolveContext& context, Node* node, unsigned& count) const
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

#ifdef STYLE_STATS
Element* SharedStyleFinder::searchDocumentForSharedStyle(const ElementResolveContext& context) const
{
    for (Element* element = context.element()->document().documentElement(); element; element = ElementTraversal::next(element)) {
        if (canShareStyleWithElement(context, element))
            return element;
    }
    return 0;
}
#endif

RenderStyle* SharedStyleFinder::locateSharedStyle(const ElementResolveContext& context, RenderStyle* newStyle)
{
    STYLE_STATS_ADD_SEARCH();
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
    if (context.element() == context.document().cssTarget())
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
    if (isShadowHost(context.element()) && context.element()->shadow()->containsActiveStyles())
        return 0;

    STYLE_STATS_ADD_ELEMENT_ELIGIBLE_FOR_SHARING();

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

#ifdef STYLE_STATS
    // FIXME: these stats don't to into account whether or not sibling/attribute
    // rules prevent these nodes from actually sharing
    if (shareElement) {
        STYLE_STATS_ADD_SEARCH_FOUND_SIBLING_FOR_SHARING();
    } else {
        shareElement = searchDocumentForSharedStyle(context);
        if (shareElement)
            STYLE_STATS_ADD_SEARCH_MISSED_SHARING();
        shareElement = 0;
    }
#endif

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return 0;

    // Can't share if sibling rules apply. This is checked at the end as it should rarely fail.
    if (m_styleResolver->styleSharingCandidateMatchesRuleSet(context, newStyle, m_siblingRuleSet))
        return 0;
    // Can't share if attribute rules apply.
    if (m_styleResolver->styleSharingCandidateMatchesRuleSet(context, newStyle, m_uncommonAttributeRuleSet))
        return 0;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (parentElementPreventsSharing(context.element()->parentElement()))
        return 0;
    STYLE_STATS_ADD_STYLE_SHARED();
    return shareElement->renderStyle();
}

}
