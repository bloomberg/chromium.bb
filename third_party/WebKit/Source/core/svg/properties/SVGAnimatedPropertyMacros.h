/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#ifndef SVGAnimatedPropertyMacros_h
#define SVGAnimatedPropertyMacros_h

#include "core/dom/Element.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "core/svg/properties/SVGAttributeToPropertyMap.h"
#include "core/svg/properties/SVGPropertyTraits.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

// SVGSynchronizableAnimatedProperty implementation
template<typename PropertyType>
struct SVGSynchronizableAnimatedProperty {
    SVGSynchronizableAnimatedProperty()
        : value(SVGPropertyTraits<PropertyType>::initialValue())
        , shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameter1>
    SVGSynchronizableAnimatedProperty(const ConstructorParameter1& value1)
        : value(value1)
        , shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameter1, typename ConstructorParameter2>
    SVGSynchronizableAnimatedProperty(const ConstructorParameter1& value1, const ConstructorParameter2& value2)
        : value(value1, value2)
        , shouldSynchronize(false)
    {
    }

    void synchronize(Element* ownerElement, const QualifiedName& attrName, const AtomicString& value)
    {
        ownerElement->setSynchronizedLazyAttribute(attrName, value);
    }

    PropertyType value;
    bool shouldSynchronize : 1;
};

// Property registration helpers
#define BEGIN_REGISTER_ANIMATED_PROPERTIES(OwnerType) \
SVGAttributeToPropertyMap& OwnerType::attributeToPropertyMap() \
{ \
    DEFINE_STATIC_LOCAL(SVGAttributeToPropertyMap, s_attributeToPropertyMap, ()); \
    return s_attributeToPropertyMap; \
} \
\
SVGAttributeToPropertyMap& OwnerType::localAttributeToPropertyMap() const \
{ \
    return attributeToPropertyMap(); \
} \
\
void OwnerType::registerAnimatedPropertiesFor##OwnerType() \
{ \
    OwnerType::m_cleanupAnimatedPropertiesCaller.setOwner(this); \
    SVGAttributeToPropertyMap& map = OwnerType::attributeToPropertyMap(); \
    if (!map.isEmpty()) \
        return; \
    typedef OwnerType UseOwnerType;

#define REGISTER_LOCAL_ANIMATED_PROPERTY(LowerProperty) \
     map.addProperty(UseOwnerType::LowerProperty##PropertyInfo());

#define REGISTER_PARENT_ANIMATED_PROPERTIES(ClassName) \
     map.addProperties(ClassName::attributeToPropertyMap()); \

#define END_REGISTER_ANIMATED_PROPERTIES }

// Property declaration helpers (used in SVG*.h files)
#define BEGIN_DECLARE_ANIMATED_PROPERTIES(OwnerType) \
public: \
    static SVGAttributeToPropertyMap& attributeToPropertyMap(); \
    virtual SVGAttributeToPropertyMap& localAttributeToPropertyMap() const; \
    void registerAnimatedPropertiesFor##OwnerType(); \
    typedef OwnerType UseOwnerType;

#define DECLARE_ANIMATED_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty) \
public: \
    static const SVGPropertyInfo* LowerProperty##PropertyInfo(); \
    bool LowerProperty##Specified() const; \
    PropertyType& LowerProperty##CurrentValue() const; \
    PropertyType& LowerProperty##BaseValue() const; \
    void set##UpperProperty##BaseValue(const PropertyType& type); \
    PassRefPtr<TearOffType> LowerProperty(); \
\
private: \
    void synchronize##UpperProperty(); \
    static PassRefPtr<SVGAnimatedProperty> lookupOrCreate##UpperProperty##Wrapper(SVGElement* maskedOwnerType); \
    static void synchronize##UpperProperty(SVGElement* maskedOwnerType); \
\
    mutable SVGSynchronizableAnimatedProperty<PropertyType> m_##LowerProperty;

#define END_DECLARE_ANIMATED_PROPERTIES \
    CleanUpAnimatedPropertiesCaller m_cleanupAnimatedPropertiesCaller;

}

#endif // SVGAnimatedPropertyMacros_h
