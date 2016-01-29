// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthPairStyleInterpolation_h
#define LengthPairStyleInterpolation_h

#include "core/CoreExport.h"
#include "core/animation/StyleInterpolation.h"
#include "platform/Length.h"

namespace blink {

class CORE_EXPORT LengthPairStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtr<LengthPairStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id, InterpolationRange range)
    {
        return adoptRef(new LengthPairStyleInterpolation(lengthPairToInterpolableValue(start), lengthPairToInterpolableValue(end), id, range));
    }

    static bool canCreateFrom(const CSSValue&);

    void apply(StyleResolverState&) const override;

private:
    LengthPairStyleInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, CSSPropertyID id, InterpolationRange range)
        : StyleInterpolation(std::move(start), std::move(end), id), m_range(range)
    {
    }

    static PassOwnPtr<InterpolableValue> lengthPairToInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthPair(InterpolableValue*, InterpolationRange);

    InterpolationRange m_range;

    friend class LengthPairStyleInterpolationTest;
};

} // namespace blink

#endif // LengthPairStyleInterpolation_h
