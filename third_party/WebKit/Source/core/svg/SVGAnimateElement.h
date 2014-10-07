/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef SVGAnimateElement_h
#define SVGAnimateElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"
#include "core/svg/SVGAnimationElement.h"
#include "wtf/OwnPtr.h"

namespace blink {

class SVGAnimatedTypeAnimator;

class SVGAnimateElement : public SVGAnimationElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<SVGAnimateElement> create(Document&);
    virtual ~SVGAnimateElement();

    virtual void trace(Visitor*) override;

    AnimatedPropertyType animatedPropertyType();
    bool animatedPropertyTypeSupportsAddition();

protected:
    SVGAnimateElement(const QualifiedName&, Document&);

    virtual void resetAnimatedType() override final;
    virtual void clearAnimatedType(SVGElement* targetElement) override final;

    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) override final;
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) override final;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) override final;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) override final;
    virtual void applyResultsToTarget() override final;
    virtual float calculateDistance(const String& fromString, const String& toString) override final;
    virtual bool isAdditive() override final;

    virtual void setTargetElement(SVGElement*) override final;
    virtual void setAttributeName(const QualifiedName&) override final;

private:
    void resetAnimatedPropertyType();
    SVGAnimatedTypeAnimator* ensureAnimator();

    virtual bool hasValidAttributeType() override;

    RefPtr<SVGPropertyBase> m_fromProperty;
    RefPtr<SVGPropertyBase> m_toProperty;
    RefPtr<SVGPropertyBase> m_toAtEndOfDurationProperty;
    RefPtr<SVGPropertyBase> m_animatedProperty;

    OwnPtrWillBeMember<SVGAnimatedTypeAnimator> m_animator;
};

inline bool isSVGAnimateElement(const SVGElement& element)
{
    return element.hasTagName(SVGNames::animateTag)
        || element.hasTagName(SVGNames::animateTransformTag)
        || element.hasTagName(SVGNames::setTag);
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGAnimateElement);

} // namespace blink

#endif // SVGAnimateElement_h
