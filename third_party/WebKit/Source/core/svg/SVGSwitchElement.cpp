/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGSwitchElement.h"

#include "SVGNames.h"
#include "core/frame/UseCounter.h"
#include "core/rendering/svg/RenderSVGTransformableContainer.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_BOOLEAN(SVGSwitchElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGSwitchElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGSwitchElement::SVGSwitchElement(Document& document)
    : SVGGraphicsElement(SVGNames::switchTag, document)
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGSwitchElement();

    UseCounter::count(document, UseCounter::SVGSwitchElement);
}

PassRefPtr<SVGSwitchElement> SVGSwitchElement::create(Document& document)
{
    return adoptRef(new SVGSwitchElement(document));
}

bool SVGSwitchElement::childShouldCreateRenderer(const Node& child) const
{
    // FIXME: This function does not do what the comment below implies it does.
    // It will create a renderer for any valid SVG element children, not just the first one.
    bool shouldCreateRenderer = false;
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (!node->isSVGElement())
            continue;

        SVGElement* element = toSVGElement(node);
        if (!element || !element->isValid())
            continue;

        shouldCreateRenderer = node == &child; // Only allow this child if it's the first valid child.
        break;
    }

    return shouldCreateRenderer && SVGGraphicsElement::childShouldCreateRenderer(child);
}

RenderObject* SVGSwitchElement::createRenderer(RenderStyle*)
{
    return new RenderSVGTransformableContainer(this);
}

}
