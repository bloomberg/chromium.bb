// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatToLayoutUnit_h
#define FloatToLayoutUnit_h

namespace blink {

#if ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)

class LayoutUnit;
class LayoutPoint;
class LayoutRect;
class LayoutSize;

using FloatWillBeLayoutUnit = LayoutUnit;
using FloatPointWillBeLayoutPoint = LayoutPoint;
using FloatRectWillBeLayoutRect = LayoutRect;
using FloatSizeWillBeLayoutSize = LayoutSize;

#define ZERO_LAYOUT_UNIT LayoutUnit(0)
#define MINUS_ONE_LAYOUT_UNIT LayoutUnit(-1)

#define LAYOUT_UNIT_TO_FLOAT(layoutUnit) (layoutUnit)
#define LAYOUT_UNIT_CEIL(layoutUnit) (layoutUnit.ceil())
#define INT_TO_LAYOUT_UNIT(i) (LayoutUnit(i))

#else // ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)

class FloatPoint;
class FloatRect;
class FloatSize;

using FloatWillBeLayoutUnit = float;
using FloatPointWillBeLayoutPoint = FloatPoint;
using FloatRectWillBeLayoutRect = FloatRect;
using FloatSizeWillBeLayoutSize = FloatSize;

#define ZERO_LAYOUT_UNIT 0
#define MINUS_ONE_LAYOUT_UNIT -1

#define LAYOUT_UNIT_TO_FLOAT(layoutUnit) (layoutUnit.toFloat())
#define LAYOUT_UNIT_CEIL(layoutUnit) (ceilf(layoutUnit))
#define INT_TO_LAYOUT_UNIT(i) (i)

#endif // ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)

} // namespace blink

#endif // FloatToLayoutUnit_h
