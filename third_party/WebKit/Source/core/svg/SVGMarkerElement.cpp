/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "core/svg/SVGMarkerElement.h"

#include "SVGNames.h"
#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/svg/SVGElementInstance.h"

namespace WebCore {

// Define custom animated property 'orientType'.
const SVGPropertyInfo* SVGMarkerElement::orientTypePropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = 0;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedEnumeration,
                                             PropertyIsReadWrite,
                                             SVGNames::orientAttr,
                                             orientTypeIdentifier(),
                                             &SVGMarkerElement::synchronizeOrientType,
                                             &SVGMarkerElement::lookupOrCreateOrientTypeWrapper);
    }
    return s_propertyInfo;
}

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGMarkerElement, SVGNames::markerUnitsAttr, MarkerUnits, markerUnits, SVGMarkerUnitsType)
DEFINE_ANIMATED_ANGLE_AND_ENUMERATION(SVGMarkerElement, SVGNames::orientAttr, orientAngleIdentifier(), OrientAngle, orientAngle)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGMarkerElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(markerUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(orientAngle)
    REGISTER_LOCAL_ANIMATED_PROPERTY(orientType)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGMarkerElement::SVGMarkerElement(Document& document)
    : SVGElement(SVGNames::markerTag, document)
    , m_refX(SVGAnimatedLength::create(this, SVGNames::refXAttr, SVGLength::create(LengthModeWidth)))
    , m_refY(SVGAnimatedLength::create(this, SVGNames::refXAttr, SVGLength::create(LengthModeWidth)))
    , m_markerWidth(SVGAnimatedLength::create(this, SVGNames::markerWidthAttr, SVGLength::create(LengthModeWidth)))
    , m_markerHeight(SVGAnimatedLength::create(this, SVGNames::markerHeightAttr, SVGLength::create(LengthModeHeight)))
    , m_viewBox(SVGAnimatedRect::create(this, SVGNames::viewBoxAttr))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
    , m_orientType(SVGMarkerOrientAngle)
    , m_markerUnits(SVGMarkerUnitsStrokeWidth)
{
    ScriptWrappable::init(this);

    // Spec: If the markerWidth/markerHeight attribute is not specified, the effect is as if a value of "3" were specified.
    m_markerWidth->setDefaultValueAsString("3");
    m_markerHeight->setDefaultValueAsString("3");

    addToPropertyMap(m_refX);
    addToPropertyMap(m_refY);
    addToPropertyMap(m_markerWidth);
    addToPropertyMap(m_markerHeight);
    addToPropertyMap(m_viewBox);

    addToPropertyMap(m_preserveAspectRatio);
    registerAnimatedPropertiesForSVGMarkerElement();
}

PassRefPtr<SVGMarkerElement> SVGMarkerElement::create(Document& document)
{
    return adoptRef(new SVGMarkerElement(document));
}

const AtomicString& SVGMarkerElement::orientTypeIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGOrientType", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

const AtomicString& SVGMarkerElement::orientAngleIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGOrientAngle", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

AffineTransform SVGMarkerElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    return SVGFitToViewBox::viewBoxToViewTransform(m_viewBox->currentValue()->value(), m_preserveAspectRatio->currentValue(), viewWidth, viewHeight);
}

bool SVGMarkerElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::markerUnitsAttr);
        supportedAttributes.add(SVGNames::refXAttr);
        supportedAttributes.add(SVGNames::refYAttr);
        supportedAttributes.add(SVGNames::markerWidthAttr);
        supportedAttributes.add(SVGNames::markerHeightAttr);
        supportedAttributes.add(SVGNames::orientAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGMarkerElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGElement::parseAttribute(name, value);
    else if (name == SVGNames::markerUnitsAttr) {
        SVGMarkerUnitsType propertyValue = SVGPropertyTraits<SVGMarkerUnitsType>::fromString(value);
        if (propertyValue > 0)
            setMarkerUnitsBaseValue(propertyValue);
    } else if (name == SVGNames::refXAttr)
        m_refX->setBaseValueAsString(value, AllowNegativeLengths, parseError);
    else if (name == SVGNames::refYAttr)
        m_refY->setBaseValueAsString(value, AllowNegativeLengths, parseError);
    else if (name == SVGNames::markerWidthAttr)
        m_markerWidth->setBaseValueAsString(value, ForbidNegativeLengths, parseError);
    else if (name == SVGNames::markerHeightAttr)
        m_markerHeight->setBaseValueAsString(value, ForbidNegativeLengths, parseError);
    else if (name == SVGNames::orientAttr) {
        SVGAngle angle;
        SVGMarkerOrientType orientType = SVGPropertyTraits<SVGMarkerOrientType>::fromString(value, angle);
        if (orientType > 0)
            setOrientTypeBaseValue(orientType);
        if (orientType == SVGMarkerOrientAngle)
            setOrientAngleBaseValue(angle);
    } else if (SVGFitToViewBox::parseAttribute(this, name, value)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::refXAttr
        || attrName == SVGNames::refYAttr
        || attrName == SVGNames::markerWidthAttr
        || attrName == SVGNames::markerHeightAttr)
        updateRelativeLengthsInformation();

    RenderSVGResourceContainer* renderer = toRenderSVGResourceContainer(this->renderer());
    if (renderer)
        renderer->invalidateCacheAndMarkForLayout();
}

void SVGMarkerElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

void SVGMarkerElement::setOrientToAuto()
{
    setOrientTypeBaseValue(SVGMarkerOrientAuto);
    setOrientAngleBaseValue(SVGAngle());

    // Mark orientAttr dirty - the next XML DOM access of that attribute kicks in synchronization.
    m_orientAngle.shouldSynchronize = true;
    m_orientType.shouldSynchronize = true;
    invalidateSVGAttributes();
    svgAttributeChanged(orientAnglePropertyInfo()->attributeName);
}

void SVGMarkerElement::setOrientToAngle(const SVGAngle& angle)
{
    setOrientTypeBaseValue(SVGMarkerOrientAngle);
    setOrientAngleBaseValue(angle);

    // Mark orientAttr dirty - the next XML DOM access of that attribute kicks in synchronization.
    m_orientAngle.shouldSynchronize = true;
    m_orientType.shouldSynchronize = true;
    invalidateSVGAttributes();
    svgAttributeChanged(orientAnglePropertyInfo()->attributeName);
}

RenderObject* SVGMarkerElement::createRenderer(RenderStyle*)
{
    return new RenderSVGResourceMarker(this);
}

bool SVGMarkerElement::selfHasRelativeLengths() const
{
    return m_refX->currentValue()->isRelative()
        || m_refY->currentValue()->isRelative()
        || m_markerWidth->currentValue()->isRelative()
        || m_markerHeight->currentValue()->isRelative();
}

void SVGMarkerElement::synchronizeOrientType(SVGElement* contextElement)
{
    ASSERT(contextElement);
    SVGMarkerElement* ownerType = toSVGMarkerElement(contextElement);
    if (!ownerType->m_orientType.shouldSynchronize)
        return;

    // If orient is not auto, the previous call to synchronizeOrientAngle already set the orientAttr to the right angle.
    if (ownerType->m_orientType.value != SVGMarkerOrientAuto)
        return;

    DEFINE_STATIC_LOCAL(AtomicString, autoString, ("auto", AtomicString::ConstructFromLiteral));
    ownerType->m_orientType.synchronize(ownerType, orientTypePropertyInfo()->attributeName, autoString);
}

PassRefPtr<SVGAnimatedProperty> SVGMarkerElement::lookupOrCreateOrientTypeWrapper(SVGElement* contextElement)
{
    ASSERT(contextElement);
    SVGMarkerElement* ownerType = toSVGMarkerElement(contextElement);
    return SVGAnimatedProperty::lookupOrCreateWrapper<SVGMarkerElement, SVGAnimatedEnumerationPropertyTearOff<SVGMarkerOrientType>, SVGMarkerOrientType>
           (ownerType, orientTypePropertyInfo(), ownerType->m_orientType.value);
}

PassRefPtr<SVGAnimatedEnumerationPropertyTearOff<SVGMarkerOrientType> > SVGMarkerElement::orientType()
{
    m_orientType.shouldSynchronize = true;
    return static_pointer_cast<SVGAnimatedEnumerationPropertyTearOff<SVGMarkerOrientType> >(lookupOrCreateOrientTypeWrapper(this));
}

}
