/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "core/svg/properties/SVGAttributeToPropertyMap.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

void SVGAttributeToPropertyMap::addProperties(const SVGAttributeToPropertyMap& map)
{
    AttributeToPropertiesMap::const_iterator end = map.m_map.end();
    for (AttributeToPropertiesMap::const_iterator it = map.m_map.begin(); it != end; ++it) {
        const PropertiesVector* mapVector = it->value.get();
        ASSERT(mapVector);

        if (!mapVector->isEmpty()) {
            const SVGPropertyInfo* firstProperty = mapVector->first();
            ASSERT(firstProperty);
            const QualifiedName& attributeName = firstProperty->attributeName;

            // All of the properties in mapVector are guaranteed to have the same attribute name.
            // Add them to our properties vector for that attribute name, reserving capacity up
            // front.
            PropertiesVector* vector = getOrCreatePropertiesVector(attributeName);
            ASSERT(vector);
            vector->reserveCapacity(vector->size() + mapVector->size());
            const PropertiesVector::const_iterator mapVectorEnd = mapVector->end();
            for (PropertiesVector::const_iterator mapVectorIt = mapVector->begin(); mapVectorIt != mapVectorEnd; ++mapVectorIt) {
                ASSERT(*mapVectorIt);
                ASSERT(attributeName == (*mapVectorIt)->attributeName);
                vector->append(*mapVectorIt);
            }
        }
    }
}

void SVGAttributeToPropertyMap::addProperty(const SVGPropertyInfo* info)
{
    ASSERT(info);
    PropertiesVector* vector = getOrCreatePropertiesVector(info->attributeName);
    ASSERT(vector);
    vector->append(info);
}

void SVGAttributeToPropertyMap::animatedPropertiesForAttribute(SVGElement* ownerType, const QualifiedName& attributeName, Vector<RefPtr<SVGAnimatedProperty> >& properties)
{
    ASSERT(ownerType);
    PropertiesVector* vector = m_map.get(attributeName);
    if (!vector)
        return;

    properties.reserveCapacity(properties.size() + vector->size());
    const PropertiesVector::iterator vectorEnd = vector->end();
    for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
        properties.append(animatedProperty(ownerType, attributeName, *vectorIt));
}

void SVGAttributeToPropertyMap::animatedPropertyTypeForAttribute(const QualifiedName& attributeName, Vector<AnimatedPropertyType>& propertyTypes)
{
    PropertiesVector* vector = m_map.get(attributeName);
    if (!vector)
        return;

    propertyTypes.reserveCapacity(propertyTypes.size() + vector->size());
    const PropertiesVector::iterator vectorEnd = vector->end();
    for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
        propertyTypes.append((*vectorIt)->animatedPropertyType);
}

void SVGAttributeToPropertyMap::synchronizeProperties(SVGElement* contextElement)
{
    ASSERT(contextElement);
    const AttributeToPropertiesMap::iterator end = m_map.end();
    for (AttributeToPropertiesMap::iterator it = m_map.begin(); it != end; ++it) {
        PropertiesVector* vector = it->value.get();
        ASSERT(vector);

        const PropertiesVector::iterator vectorEnd = vector->end();
        for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
            synchronizeProperty(contextElement, it->key, *vectorIt);
    }
}

bool SVGAttributeToPropertyMap::synchronizeProperty(SVGElement* contextElement, const QualifiedName& attributeName)
{
    ASSERT(contextElement);
    PropertiesVector* vector = m_map.get(attributeName);
    if (!vector)
        return false;

    const PropertiesVector::iterator vectorEnd = vector->end();
    for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
        synchronizeProperty(contextElement, attributeName, *vectorIt);

    return true;
}

SVGAttributeToPropertyMap::PropertiesVector* SVGAttributeToPropertyMap::getOrCreatePropertiesVector(const QualifiedName& attributeName)
{
    ASSERT(attributeName != anyQName());
    AttributeToPropertiesMap::AddResult addResult = m_map.add(attributeName, PassOwnPtr<PropertiesVector>());
    PropertiesVector* vector = addResult.iterator->value.get();
    if (addResult.isNewEntry) {
        ASSERT(!vector);
        vector = (addResult.iterator->value = adoptPtr(new PropertiesVector)).get();
    }
    ASSERT(vector);
    ASSERT(addResult.iterator->value.get() == vector);
    return vector;
}

void SVGAttributeToPropertyMap::synchronizeProperty(SVGElement* contextElement, const QualifiedName& attributeName, const SVGPropertyInfo* info)
{
    ASSERT(info);
    ASSERT_UNUSED(attributeName, attributeName == info->attributeName);
    ASSERT(info->synchronizeProperty);
    (*info->synchronizeProperty)(contextElement);
}

PassRefPtr<SVGAnimatedProperty> SVGAttributeToPropertyMap::animatedProperty(SVGElement* contextElement, const QualifiedName& attributeName, const SVGPropertyInfo* info)
{
    ASSERT(info);
    ASSERT_UNUSED(attributeName, attributeName == info->attributeName);
    ASSERT(info->lookupOrCreateWrapperForAnimatedProperty);
    return (*info->lookupOrCreateWrapperForAnimatedProperty)(contextElement);
}

}
