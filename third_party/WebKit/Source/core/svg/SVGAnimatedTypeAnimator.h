/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
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

#ifndef SVGAnimatedTypeAnimator_h
#define SVGAnimatedTypeAnimator_h

#include "core/svg/SVGElementInstance.h"
#include "core/svg/properties/NewSVGProperty.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

struct SVGElementAnimatedProperties {
    SVGElementAnimatedProperties();

    SVGElementAnimatedProperties(SVGElement*, Vector<RefPtr<SVGAnimatedProperty> >&);

    SVGElement* element;
    Vector<RefPtr<SVGAnimatedProperty> > properties;
};
typedef Vector<SVGElementAnimatedProperties> SVGElementAnimatedPropertyList;

class SVGAnimationElement;

class SVGAnimatedTypeAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGAnimatedTypeAnimator();
    virtual PassRefPtr<NewSVGPropertyBase> constructFromString(const String&) = 0;

    virtual PassRefPtr<NewSVGPropertyBase> startAnimValAnimation(const SVGElementAnimatedPropertyList&) = 0;
    virtual void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) = 0;
    virtual PassRefPtr<NewSVGPropertyBase> resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&) = 0;
    virtual void animValWillChange(const SVGElementAnimatedPropertyList&) = 0;
    virtual void animValDidChange(const SVGElementAnimatedPropertyList&) = 0;

    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, NewSVGPropertyBase*, NewSVGPropertyBase*, NewSVGPropertyBase*, NewSVGPropertyBase*) = 0;
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;

    void calculateFromAndToValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& toString);
    void calculateFromAndByValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& byString);

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }
    AnimatedPropertyType type() const { return m_type; }

    SVGElementAnimatedPropertyList findAnimatedPropertiesForAttributeName(SVGElement*, const QualifiedName&);

protected:
    SVGAnimatedTypeAnimator(AnimatedPropertyType, SVGAnimationElement*, SVGElement*);

    AnimatedPropertyType m_type;
    SVGAnimationElement* m_animationElement;
    SVGElement* m_contextElement;
};

} // namespace WebCore

#endif // SVGAnimatedTypeAnimator_h
