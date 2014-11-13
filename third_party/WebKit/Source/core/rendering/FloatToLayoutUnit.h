// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatToLayoutUnit_h
#define FloatToLayoutUnit_h

#include "platform/LayoutUnit.h"

namespace blink {

class FloatPoint;
class FloatRect;
class FloatSize;
class LayoutUnit;
class LayoutPoint;
class LayoutRect;
class LayoutSize;

class FloatLineLayoutUnit {
public:
    FloatLineLayoutUnit() : m_value(0) { }
    FloatLineLayoutUnit(float f) : m_value(f) { }

    float toFloat() const
    {
        return m_value;
    }

    LayoutUnit toLayoutUnit() const
    {
        return LayoutUnit(m_value);
    }

public:
    float m_value;
};

class LayoutUnitLineLayoutUnit {
public:
    LayoutUnitLineLayoutUnit() { }
    LayoutUnitLineLayoutUnit(LayoutUnit f) : m_value(f) { }

    float toFloat() const
    {
        return m_value.toFloat();
    }

    LayoutUnit toLayoutUnit() const
    {
        return m_value;
    }

public:
    LayoutUnit m_value;
};

#if ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)
using LineLayoutUnit = LayoutUnitLineLayoutUnit;
#else // ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)
using LineLayoutUnit = FloatLineLayoutUnit;
#endif // ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)

using FloatWillBeLayoutUnit = float;
using FloatPointWillBeLayoutPoint = FloatPoint;
using FloatRectWillBeLayoutRect = FloatRect;
using FloatSizeWillBeLayoutSize = FloatSize;

} // namespace blink

#endif // FloatToLayoutUnit_h
