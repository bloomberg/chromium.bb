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

#ifndef SVGNumberOptionalNumber_h
#define SVGNumberOptionalNumber_h

#include "core/svg/SVGNumber.h"
#include "core/svg/SVGParsingError.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGNumberOptionalNumber : public SVGPropertyBase {
public:
    // Tearoff of SVGNumberOptionalNumber is never created.
    typedef void TearOffType;
    typedef void PrimitiveType;

    static RawPtr<SVGNumberOptionalNumber> create(RawPtr<SVGNumber> firstNumber, RawPtr<SVGNumber> secondNumber)
    {
        return new SVGNumberOptionalNumber(firstNumber, secondNumber);
    }

    RawPtr<SVGNumberOptionalNumber> clone() const;
    RawPtr<SVGPropertyBase> cloneForAnimation(const String&) const override;

    String valueAsString() const override;
    SVGParsingError setValueAsString(const String&);

    void add(RawPtr<SVGPropertyBase>, SVGElement*) override;
    void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, RawPtr<SVGPropertyBase> from, RawPtr<SVGPropertyBase> to, RawPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) override;
    float calculateDistance(RawPtr<SVGPropertyBase> to, SVGElement* contextElement) override;

    static AnimatedPropertyType classType() { return AnimatedNumberOptionalNumber; }

    RawPtr<SVGNumber> firstNumber() const { return m_firstNumber; }
    RawPtr<SVGNumber> secondNumber() const { return m_secondNumber; }

    DECLARE_VIRTUAL_TRACE();

protected:
    SVGNumberOptionalNumber(RawPtr<SVGNumber> firstNumber, RawPtr<SVGNumber> secondNumber);

    Member<SVGNumber> m_firstNumber;
    Member<SVGNumber> m_secondNumber;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGNumberOptionalNumber);

} // namespace blink

#endif // SVGNumberOptionalNumber_h
