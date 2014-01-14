/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGBoolean_h
#define SVGBoolean_h

#include "core/svg/properties/NewSVGProperty.h"

namespace WebCore {

class SVGBoolean : public NewSVGPropertyBase {
public:
    // SVGBoolean does not have a tear-off type.
    // The below typedef is used by NewSVGAnimatedProperty.
    typedef void TearOffType;
    typedef bool PrimitiveType;

    static PassRefPtr<SVGBoolean> create(bool value = false)
    {
        return adoptRef(new SVGBoolean(value));
    }

    PassRefPtr<SVGBoolean> clone() const { return create(m_value); }
    virtual PassRefPtr<NewSVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);

    virtual void add(PassRefPtr<NewSVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<NewSVGPropertyBase> from, PassRefPtr<NewSVGPropertyBase> to, PassRefPtr<NewSVGPropertyBase> toAtEndOfDurationValue, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<NewSVGPropertyBase> to, SVGElement*) OVERRIDE;

    bool operator==(const SVGBoolean& other) const { return m_value == other.m_value; }
    bool operator!=(const SVGBoolean& other) const { return !operator==(other); }

    bool value() const { return m_value; }
    void setValue(bool value) { m_value = value; }

    static AnimatedPropertyType classType() { return AnimatedBoolean; }

private:
    SVGBoolean(bool value)
        : NewSVGPropertyBase(classType())
        , m_value(value)
    {
    }

    bool m_value;
};

inline PassRefPtr<SVGBoolean> toSVGBoolean(PassRefPtr<NewSVGPropertyBase> passBase)
{
    RefPtr<NewSVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGBoolean::classType());
    return static_pointer_cast<SVGBoolean>(base.release());
}

} // namespace WebCore

#endif // SVGBoolean_h
