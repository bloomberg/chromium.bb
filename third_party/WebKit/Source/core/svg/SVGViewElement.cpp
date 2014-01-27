/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGViewElement.h"


namespace WebCore {

// Animated property definitions

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGViewElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGViewElement::SVGViewElement(Document& document)
    : SVGElement(SVGNames::viewTag, document)
    , m_viewBox(SVGAnimatedRect::create(this, SVGNames::viewBoxAttr))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
    , m_zoomAndPan(SVGZoomAndPanMagnify)
    , m_viewTarget(SVGNames::viewTargetAttr)
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_viewBox);
    addToPropertyMap(m_preserveAspectRatio);
    registerAnimatedPropertiesForSVGViewElement();
}

PassRefPtr<SVGViewElement> SVGViewElement::create(Document& document)
{
    return adoptRef(new SVGViewElement(document));
}

bool SVGViewElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        SVGZoomAndPan::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::viewTargetAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGViewElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::viewTargetAttr) {
        viewTarget().reset(value);
        return;
    }

    if (SVGFitToViewBox::parseAttribute(this, name, value))
        return;
    if (SVGZoomAndPan::parseAttribute(this, name, value))
        return;

    ASSERT_NOT_REACHED();
}

}
