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
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
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

bool SharedStyleFinder::canShareStyleWithControl(Element& candidate) const
{
    if (!candidate.hasTagName(inputTag) || !element().hasTagName(inputTag))
        return false;

    HTMLInputElement& candidateInput = toHTMLInputElement(candidate);
    HTMLInputElement& thisInput = toHTMLInputElement(element());

    if (candidateInput.isAutofilled() != thisInput.isAutofilled())
        return false;
    if (candidateInput.shouldAppearChecked() != thisInput.shouldAppearChecked())
        return false;
    if (candidateInput.shouldAppearIndeterminate() != thisInput.shouldAppearIndeterminate())
        return false;
    if (candidateInput.isRequired() != thisInput.isRequired())
        return false;

    if (candidate.isDisabledFormControl() != element().isDisabledFormControl())
        return false;

    if (candidate.isDefaultButtonForForm() != element().isDefaultButtonForForm())
        return false;

    if (document().containsValidityStyleRules()) {
        bool willValidate = candidate.willValidate();

        if (willValidate != element().willValidate())
            return false;

        if (willValidate && (candidate.isValidFormControlElement() != element().isValidFormControlElement()))
            return false;

        if (candidate.isInRange() != element().isInRange())
            return false;

        if (candidate.isOutOfRange() != element().isOutOfRange())
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

static inline const AtomicString& typeAttributeValue(const Element& element)
{
    // type is animatable in SVG so we need to go down the slow path here.
    return element.isSVGElement() ? element.getAttribute(typeAttr) : element.fastGetAttribute(typeAttr);
}

bool SharedStyleFinder::sharingCandidateHasIdenticalStyleAffectingAttributes(Element& candidate) const
{
    if (element().elementData() == candidate.elementData())
        return true;
    if (element().fastGetAttribute(XMLNames::langAttr) != candidate.fastGetAttribute(XMLNames::langAttr))
        return false;
    if (element().fastGetAttribute(langAttr) != candidate.fastGetAttribute(langAttr))
        return false;

    // These two checks must be here since RuleSet has a specail case to allow style sharing between elements
    // with type and readonly attributes whereas other attribute selectors prevent sharing.
    if (typeAttributeValue(element()) != typeAttributeValue(candidate))
        return false;
    if (element().fastGetAttribute(readonlyAttr) != candidate.fastGetAttribute(readonlyAttr))
        return false;

    if (!m_elementAffectedByClassRules) {
        if (candidate.hasClass() && classNamesAffectedByRules(candidate.classNames()))
            return false;
    } else if (candidate.hasClass()) {
        // SVG elements require a (slow!) getAttribute comparision because "class" is an animatable attribute for SVG.
        if (element().isSVGElement()) {
            if (element().getAttribute(classAttr) != candidate.getAttribute(classAttr))
                return false;
        } else if (element().classNames() != candidate.classNames()) {
            return false;
        }
    } else {
        return false;
    }

    if (element().presentationAttributeStyle() != candidate.presentationAttributeStyle())
        return false;

    // FIXME: Consider removing this, it's unlikely we'll have so many progress elements
    // that sharing the style makes sense. Instead we should just not support style sharing
    // for them.
    if (element().hasTagName(progressTag)) {
        if (element().shouldAppearIndeterminate() != candidate.shouldAppearIndeterminate())
            return false;
    }

    return true;
}

bool SharedStyleFinder::canShareStyleWithElement(Element& candidate) const
{
    if (element() == candidate)
        return false;
    Element* parent = candidate.parentElement();
    RenderStyle* style = candidate.renderStyle();
    if (!style)
        return false;
    if (!parent)
        return false;
    if (element().parentElement()->renderStyle() != parent->renderStyle())
        return false;
    if (style->unique())
        return false;
    if (style->hasUniquePseudoStyle())
        return false;
    if (candidate.tagQName() != element().tagQName())
        return false;
    if (candidate.inlineStyle())
        return false;
    if (candidate.needsStyleRecalc())
        return false;
    if (candidate.isSVGElement() && toSVGElement(candidate).animatedSMILStyleProperties())
        return false;
    if (candidate.isLink() != element().isLink())
        return false;
    if (candidate.hovered() != element().hovered())
        return false;
    if (candidate.active() != element().active())
        return false;
    if (candidate.focused() != element().focused())
        return false;
    if (candidate.shadowPseudoId() != element().shadowPseudoId())
        return false;
    if (candidate == document().cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(candidate))
        return false;
    if (candidate.additionalPresentationAttributeStyle() != element().additionalPresentationAttributeStyle())
        return false;
    if (candidate.hasID() && m_features.idsInRules.contains(candidate.idForStyleResolution().impl()))
        return false;
    if (candidate.hasScopedHTMLStyleChild())
        return false;
    if (candidate.shadow() && candidate.shadow()->containsActiveStyles())
        return 0;

    // FIXME: We should share style for option and optgroup whenever possible.
    // Before doing so, we need to resolve issues in HTMLSelectElement::recalcListItems
    // and RenderMenuList::setText. See also https://bugs.webkit.org/show_bug.cgi?id=88405
    if (candidate.hasTagName(optionTag) || candidate.hasTagName(optgroupTag))
        return false;

    bool isControl = candidate.isFormControlElement();

    if (isControl != element().isFormControlElement())
        return false;

    if (isControl && !canShareStyleWithControl(candidate))
        return false;

    if (style->transitions() || style->animations())
        return false;

    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    if (candidate.hasTagName(iframeTag) || candidate.hasTagName(frameTag) || candidate.hasTagName(embedTag) || candidate.hasTagName(objectTag) || candidate.hasTagName(appletTag) || candidate.hasTagName(canvasTag))
        return false;

    // FIXME: This line is surprisingly hot, we may wish to inline hasDirectionAuto into StyleResolver.
    if (candidate.isHTMLElement() && toHTMLElement(candidate).hasDirectionAuto())
        return false;

    if (candidate.isLink() && m_context.elementLinkState() != style->insideLink())
        return false;

    if (candidate.isUnresolvedCustomElement() != element().isUnresolvedCustomElement())
        return false;

    // Deny sharing styles between WebVTT and non-WebVTT nodes.
    if (candidate.isWebVTTElement() != element().isWebVTTElement())
        return false;

    if (candidate.isWebVTTElement() && element().isWebVTTElement() && toWebVTTElement(candidate).isPastNode() != toWebVTTElement(element()).isPastNode())
        return false;

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(&document())) {
        if (candidate == fullscreen->webkitCurrentFullScreenElement() || element() == fullscreen->webkitCurrentFullScreenElement())
            return false;
    }

    if (element().parentElement() != parent) {
        if (!parent->isStyledElement())
            return false;
        if (parent->hasScopedHTMLStyleChild())
            return false;
        if (parent->inlineStyle())
            return false;
        if (parent->isSVGElement() && toSVGElement(parent)->animatedSMILStyleProperties())
            return false;
        if (parent->hasID() && m_features.idsInRules.contains(parent->idForStyleResolution().impl()))
            return false;
        if (!parent->childrenSupportStyleSharing())
            return false;
    }

    return true;
}

bool SharedStyleFinder::documentContainsValidCandidate() const
{
    for (Element* element = document().documentElement(); element; element = ElementTraversal::next(element)) {
        if (canShareStyleWithElement(*element))
            return true;
    }
    return false;
}

inline Element* SharedStyleFinder::findElementForStyleSharing() const
{
    StyleSharingList& styleSharingList = m_styleResolver.styleSharingList();
    for (StyleSharingList::iterator it = styleSharingList.begin(); it != styleSharingList.end(); ++it) {
        if (!canShareStyleWithElement(**it))
            continue;
        Element* element = it->get();
        if (it != styleSharingList.begin()) {
            // Move the element to the front of the LRU
            styleSharingList.remove(it);
            styleSharingList.prepend(element);
        }
        return element;
    }
    m_styleResolver.addToStyleSharingList(&element());
    return 0;
}

bool SharedStyleFinder::matchesRuleSet(RuleSet* ruleSet)
{
    if (!ruleSet)
        return false;
    ElementRuleCollector collector(m_context, m_styleResolver.selectorFilter());
    return collector.hasAnyMatchingRules(ruleSet);
}

RenderStyle* SharedStyleFinder::findSharedStyle()
{
    STYLE_STATS_ADD_SEARCH();

    if (!element().supportsStyleSharing())
        return 0;

    STYLE_STATS_ADD_ELEMENT_ELIGIBLE_FOR_SHARING();

    // Cache whether context.element() is affected by any known class selectors.
    m_elementAffectedByClassRules = element().hasClass() && classNamesAffectedByRules(element().classNames());

    Element* shareElement = findElementForStyleSharing();

#ifdef STYLE_STATS
    // FIXME: these stats don't to into account whether or not sibling/attribute
    // rules prevent these nodes from actually sharing
    if (shareElement)
        STYLE_STATS_ADD_SEARCH_FOUND_SIBLING_FOR_SHARING();
    else if (documentContainsValidCandidate())
        STYLE_STATS_ADD_SEARCH_MISSED_SHARING();
#endif

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return 0;

    // Can't share if sibling or attribute rules apply. This is checked at the end as it should rarely fail.
    if (matchesRuleSet(m_siblingRuleSet) || matchesRuleSet(m_uncommonAttributeRuleSet))
        return 0;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (element().parentElement()->childrenSupportStyleSharing())
        return 0;
    STYLE_STATS_ADD_STYLE_SHARED();
    return shareElement->renderStyle();
}

}
