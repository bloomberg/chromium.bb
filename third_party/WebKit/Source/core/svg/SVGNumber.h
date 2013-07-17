/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef SVGNumber_h
#define SVGNumber_h

#include "core/svg/properties/SVGPropertyTraits.h"

namespace WebCore {

class SVGNumber {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGNumber()
        : m_value(0)
    {
    }

    SVGNumber(float value)
        : m_value(value)
    {
    }

    SVGNumber& operator+=(const SVGNumber& rhs)
    {
        m_value += rhs.value();
        return *this;
    }

    float value() const { return m_value; }
    float& valueRef() { return m_value; }
    String valueAsString() const { return String::number(m_value); }
    void setValue(float value) { m_value = value; }

private:
    float m_value;
};

COMPILE_ASSERT(sizeof(SVGNumber) == sizeof(float), SVGNumber_same_size_as_float);

template<>
struct SVGPropertyTraits<SVGNumber> {
    static SVGNumber initialValue() { return SVGNumber(); }
    static String toString(const SVGNumber& type) { return type.valueAsString(); }
};

} // namespace WebCore

#endif // SVGNumber_h
