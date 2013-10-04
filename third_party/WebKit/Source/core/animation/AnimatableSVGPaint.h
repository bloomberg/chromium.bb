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

#ifndef AnimatableSVGPaint_h
#define AnimatableSVGPaint_h

#include "core/animation/AnimatableColor.h"
#include "core/animation/AnimatableValue.h"
#include "core/svg/SVGPaint.h"

namespace WebCore {

class AnimatableSVGPaint : public AnimatableValue {
public:
    virtual ~AnimatableSVGPaint() { }
    static PassRefPtr<AnimatableSVGPaint> create(SVGPaint::SVGPaintType type, const Color& color, const String& uri)
    {
        return create(type, AnimatableColorImpl(color), uri);
    }
    static PassRefPtr<AnimatableSVGPaint> create(SVGPaint::SVGPaintType type, const AnimatableColorImpl& color, const String& uri)
    {
        return adoptRef(new AnimatableSVGPaint(type, color, uri));
    }
    SVGPaint::SVGPaintType paintType() const { return m_type; };
    Color color() const { return m_color.toColor(); };
    const String& uri() const { return m_uri; };

protected:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;

private:
    AnimatableSVGPaint(SVGPaint::SVGPaintType type, const AnimatableColorImpl& color, const String& uri)
        : m_type(type)
        , m_color(color)
        , m_uri(uri)
    {
    }
    virtual AnimatableType type() const OVERRIDE { return TypeSVGPaint; }

    SVGPaint::SVGPaintType m_type;
    AnimatableColorImpl m_color;
    String m_uri;
};

inline const AnimatableSVGPaint* toAnimatableSVGPaint(const AnimatableValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value && value->isSVGPaint());
    return static_cast<const AnimatableSVGPaint*>(value);
}

} // namespace WebCore

#endif // AnimatableSVGPaint_h
