/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGAnimatedNewPropertyAnimator_h
#define SVGAnimatedNewPropertyAnimator_h

#include "core/svg/SVGAnimatedTypeAnimator.h"

namespace WebCore {

class NewSVGAnimatedPropertyBase;

// Bridges new SVGProperty impl. to existing SVG animation impl.
class SVGAnimatedNewPropertyAnimator FINAL : public SVGAnimatedTypeAnimator {
public:
    SVGAnimatedNewPropertyAnimator(AnimatedPropertyType, SVGAnimationElement*, SVGElement*);
    virtual ~SVGAnimatedNewPropertyAnimator();

    PassRefPtr<NewSVGPropertyBase> createPropertyForAnimation(const String&);

    // SVGAnimatedTypeAnimator:
    virtual PassRefPtr<NewSVGPropertyBase> constructFromString(const String&) OVERRIDE;

    virtual PassRefPtr<NewSVGPropertyBase> startAnimValAnimation(const SVGElementAnimatedPropertyList&) OVERRIDE;
    virtual void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) OVERRIDE;
    virtual PassRefPtr<NewSVGPropertyBase> resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&) OVERRIDE;
    virtual void animValWillChange(const SVGElementAnimatedPropertyList&) OVERRIDE;
    virtual void animValDidChange(const SVGElementAnimatedPropertyList&) OVERRIDE;

    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, NewSVGPropertyBase*, NewSVGPropertyBase*, NewSVGPropertyBase*, NewSVGPropertyBase*) OVERRIDE;
    virtual float calculateDistance(const String& fromString, const String& toString) OVERRIDE;

private:
    PassRefPtr<NewSVGPropertyBase> resetAnimation(const SVGElementAnimatedPropertyList&);

    bool isAnimatingSVGDom() const { return m_animatedProperty; }
    bool isAnimatingCSSProperty() const { return !m_animatedProperty; }

    RefPtr<NewSVGAnimatedPropertyBase> m_animatedProperty;
};

} // namespace WebCore

#endif // SVGAnimatedNewPropertyAnimator_h
