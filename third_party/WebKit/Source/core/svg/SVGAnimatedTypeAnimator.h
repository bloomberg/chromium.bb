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

#include "core/svg/properties/SVGPropertyInfo.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class NewSVGAnimatedPropertyBase;
class NewSVGPropertyBase;
class SVGElement;
class SVGAnimationElement;

class SVGAnimatedTypeAnimator FINAL {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<SVGAnimatedTypeAnimator> create(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* targetElement)
    {
        return adoptPtr(new SVGAnimatedTypeAnimator(type, animationElement, targetElement));
    }
    ~SVGAnimatedTypeAnimator();

    PassRefPtr<NewSVGPropertyBase> constructFromString(const String&);

    PassRefPtr<NewSVGPropertyBase> startAnimValAnimation(const Vector<SVGElement*>&);
    void stopAnimValAnimation(const Vector<SVGElement*>&);
    PassRefPtr<NewSVGPropertyBase> resetAnimValToBaseVal(const Vector<SVGElement*>&);

    void calculateAnimatedValue(float percentage, unsigned repeatCount, NewSVGPropertyBase*, NewSVGPropertyBase*, NewSVGPropertyBase*, NewSVGPropertyBase*);
    float calculateDistance(const String& fromString, const String& toString);

    void calculateFromAndToValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& toString);
    void calculateFromAndByValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& byString);

    void setContextElement(SVGElement* contextElement) { m_contextElement = contextElement; }
    AnimatedPropertyType type() const { return m_type; }

private:
    SVGAnimatedTypeAnimator(AnimatedPropertyType, SVGAnimationElement*, SVGElement*);

    friend class ParsePropertyFromString;
    PassRefPtr<NewSVGPropertyBase> createPropertyForAnimation(const String&);
    PassRefPtr<NewSVGPropertyBase> resetAnimation(const Vector<SVGElement*>&);

    bool isAnimatingSVGDom() const { return m_animatedProperty; }
    bool isAnimatingCSSProperty() const { return !m_animatedProperty; }

    AnimatedPropertyType m_type;
    SVGAnimationElement* m_animationElement;
    SVGElement* m_contextElement;
    RefPtr<NewSVGAnimatedPropertyBase> m_animatedProperty;
};

} // namespace WebCore

#endif // SVGAnimatedTypeAnimator_h
