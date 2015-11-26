// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingLengthChecker_h
#define UnderlyingLengthChecker_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/UnderlyingValue.h"

namespace blink {

class UnderlyingLengthChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<UnderlyingLengthChecker> create(const InterpolationType& type, size_t underlyingLength)
    {
        return adoptPtr(new UnderlyingLengthChecker(type, underlyingLength));
    }

    static size_t getUnderlyingLength(const UnderlyingValue& underlyingValue)
    {
        if (!underlyingValue)
            return 0;
        return toInterpolableList(underlyingValue->interpolableValue()).length();
    }

    bool isValid(const InterpolationEnvironment&, const UnderlyingValue& underlyingValue) const final
    {
        return m_underlyingLength == getUnderlyingLength(underlyingValue);
    }

private:
    UnderlyingLengthChecker(const InterpolationType& type, size_t underlyingLength)
        : ConversionChecker(type)
        , m_underlyingLength(underlyingLength)
    {}

    size_t m_underlyingLength;
};

} // namespace blink

#endif // UnderlyingLengthChecker_h
