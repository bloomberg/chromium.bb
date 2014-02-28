/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef SVGAnimatedProperty_h
#define SVGAnimatedProperty_h

#include "core/svg/properties/SVGAnimatedPropertyDescription.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class SVGElement;

class SVGAnimatedProperty : public RefCounted<SVGAnimatedProperty> {
public:
    SVGElement* contextElement() const { return m_contextElement; }
    void resetContextElement() { m_contextElement = 0; }
    const QualifiedName& attributeName() const { return m_attributeName; }
    AnimatedPropertyType animatedPropertyType() const { return m_animatedPropertyType; }
    bool isAnimating() const { return m_isAnimating; }
    bool isReadOnly() const { return m_isReadOnly; }
    void setIsReadOnly() { m_isReadOnly = true; }

    void commitChange();

    virtual bool isAnimatedListTearOff() const { return false; }

    // Caching facilities.
    typedef HashMap<SVGAnimatedPropertyDescription, RefPtr<SVGAnimatedProperty>, SVGAnimatedPropertyDescriptionHash, SVGAnimatedPropertyDescriptionHashTraits> Cache;

    virtual ~SVGAnimatedProperty();

protected:
    SVGAnimatedProperty(SVGElement*, const QualifiedName&, AnimatedPropertyType);

private:
    SVGElement* m_contextElement;
    const QualifiedName& m_attributeName;
    AnimatedPropertyType m_animatedPropertyType;

protected:
    bool m_isAnimating;
    bool m_isReadOnly;
};

}

#endif // SVGAnimatedProperty_h
