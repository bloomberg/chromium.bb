// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConstantStyleInterpolation_h
#define ConstantStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

class ConstantStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtr<ConstantStyleInterpolation> create(CSSValue* value, CSSPropertyID id)
    {
        return adoptRef(new ConstantStyleInterpolation(value, id));
    }

    void apply(StyleResolverState& state) const override
    {
        StyleBuilder::applyProperty(m_id, state, m_value.get());
    }

private:
    ConstantStyleInterpolation(CSSValue* value, CSSPropertyID id)
        : StyleInterpolation(InterpolableList::create(0), InterpolableList::create(0), id)
        , m_value(value)
    { }

    RefPtrWillBePersistent<CSSValue> m_value;
};

}

#endif
