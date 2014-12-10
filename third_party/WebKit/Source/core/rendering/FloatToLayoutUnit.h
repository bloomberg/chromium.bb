// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// The classes in this file are transitional types intended to facilitate
// moving the LineBox types from float-based units to LayoutUnits. At first,
// these types will be backed by floats, and then swapped to LayoutUnit
// implementations, and eventually removed when LayoutUnits in this code
// are stabilized. See
// https://docs.google.com/a/chromium.org/document/d/1fro9Drq78rYBwr6K9CPK-y0TDSVxlBuXl6A54XnKAyE/edit
// for details.

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

    FloatLineLayoutUnit(const LayoutUnit &layoutUnit) : m_value(layoutUnit.toFloat()) { }

    operator float() const { return toFloat(); }
    operator LayoutUnit() const { return toLayoutUnit(); }

    float rawValue() const
    {
        return m_value;
    }

    float toFloat() const
    {
        return m_value;
    }

    LayoutUnit toLayoutUnit() const
    {
        return LayoutUnit(m_value);
    }

private:
    float m_value;
};

inline FloatLineLayoutUnit operator-(const FloatLineLayoutUnit& a)
{
    FloatLineLayoutUnit returnVal(-a.rawValue());
    return returnVal;
}

inline bool operator<=(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return a.rawValue() <= b.rawValue();
}

inline bool operator<=(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() <= b;
}

inline bool operator<=(const FloatLineLayoutUnit& a, int b)
{
    return a <= FloatLineLayoutUnit(b);
}

inline bool operator<=(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a <= FloatLineLayoutUnit(b);
}

inline bool operator<=(float a, const FloatLineLayoutUnit& b)
{
    return a <= b.toFloat();
}

inline bool operator<=(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) <= b;
}

inline bool operator<=(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) <= b;
}

inline bool operator>=(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return a.rawValue() >= b.rawValue();
}

inline bool operator>=(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() >= b;
}

inline bool operator>=(const FloatLineLayoutUnit& a, int b)
{
    return a >= FloatLineLayoutUnit(b);
}

inline bool operator>=(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a >= FloatLineLayoutUnit(b);
}

inline bool operator>=(float a, const FloatLineLayoutUnit& b)
{
    return a >= b.toFloat();
}

inline bool operator>=(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) >= b;
}

inline bool operator>=(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) >= b;
}

inline bool operator<(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return a.rawValue() < b.rawValue();
}

inline bool operator<(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() < b;
}

inline bool operator<(const FloatLineLayoutUnit& a, int b)
{
    return a < FloatLineLayoutUnit(b);
}

inline bool operator<(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a < FloatLineLayoutUnit(b);
}

inline bool operator<(float a, const FloatLineLayoutUnit& b)
{
    return a < b.toFloat();
}

inline bool operator<(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) < b;
}

inline bool operator<(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) < b;
}

inline bool operator>(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return a.rawValue() > b.rawValue();
}

inline bool operator>(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() > b;
}

inline bool operator>(const FloatLineLayoutUnit& a, int b)
{
    return a > FloatLineLayoutUnit(b);
}

inline bool operator>(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a > FloatLineLayoutUnit(b);
}

inline bool operator>(float a, const FloatLineLayoutUnit& b)
{
    return a > b.toFloat();
}

inline bool operator>(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) > b;
}

inline bool operator>(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) > b;
}

inline bool operator!=(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return a.rawValue() != b.rawValue();
}

inline bool operator!=(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() != b;
}

inline bool operator!=(const FloatLineLayoutUnit& a, int b)
{
    return a != FloatLineLayoutUnit(b);
}

inline bool operator!=(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a != FloatLineLayoutUnit(b);
}

inline bool operator!=(float a, const FloatLineLayoutUnit& b)
{
    return a != b.toFloat();
}

inline bool operator!=(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) != b;
}

inline bool operator!=(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) != b;
}

inline bool operator==(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return a.rawValue() == b.rawValue();
}

inline bool operator==(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() == b;
}

inline bool operator==(const FloatLineLayoutUnit& a, int b)
{
    return a == FloatLineLayoutUnit(b);
}

inline bool operator==(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a == FloatLineLayoutUnit(b);
}

inline bool operator==(float a, const FloatLineLayoutUnit& b)
{
    return a == b.toFloat();
}

inline bool operator==(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) == b;
}

inline bool operator==(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) == b;
}

inline FloatLineLayoutUnit operator*(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a.rawValue() * b.rawValue());
}

inline float operator*(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() * b;
}

inline FloatLineLayoutUnit operator*(const FloatLineLayoutUnit& a, int b)
{
    return a * FloatLineLayoutUnit(b);
}

inline FloatLineLayoutUnit operator*(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a * FloatLineLayoutUnit(b);
}

inline float operator*(float a, const FloatLineLayoutUnit& b)
{
    return a * b.toFloat();
}

inline FloatLineLayoutUnit operator*(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) * b;
}

inline FloatLineLayoutUnit operator*(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) * b;
}

inline FloatLineLayoutUnit operator/(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a.rawValue() / b.rawValue());
}

inline float operator/(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() / b;
}

inline FloatLineLayoutUnit operator/(const FloatLineLayoutUnit& a, int b)
{
    return a / FloatLineLayoutUnit(b);
}

inline FloatLineLayoutUnit operator/(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a / FloatLineLayoutUnit(b);
}

inline float operator/(float a, const FloatLineLayoutUnit& b)
{
    return a / b.toFloat();
}

inline FloatLineLayoutUnit operator/(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) / b;
}

inline FloatLineLayoutUnit operator/(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) / b;
}

inline FloatLineLayoutUnit operator+(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a.rawValue() + b.rawValue());
}

inline float operator+(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() + b;
}

inline FloatLineLayoutUnit operator+(const FloatLineLayoutUnit& a, int b)
{
    return a + FloatLineLayoutUnit(b);
}

inline FloatLineLayoutUnit operator+(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a + FloatLineLayoutUnit(b);
}

inline float operator+(float a, const FloatLineLayoutUnit& b)
{
    return a + b.toFloat();
}

inline FloatLineLayoutUnit operator+(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) + b;
}

inline FloatLineLayoutUnit operator+(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) + b;
}

inline FloatLineLayoutUnit operator-(const FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a.rawValue() - b.rawValue());
}

inline float operator-(const FloatLineLayoutUnit& a, float b)
{
    return a.toFloat() - b;
}

inline FloatLineLayoutUnit operator-(const FloatLineLayoutUnit& a, int b)
{
    return a - FloatLineLayoutUnit(b);
}

inline FloatLineLayoutUnit operator-(const FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    return a - FloatLineLayoutUnit(b);
}

inline float operator-(float a, const FloatLineLayoutUnit& b)
{
    return a - b.toFloat();
}

inline FloatLineLayoutUnit operator-(int a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) - b;
}

inline FloatLineLayoutUnit operator-(const LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    return FloatLineLayoutUnit(a) - b;
}

inline FloatLineLayoutUnit& operator*=(FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a * b;
    return a;
}

inline FloatLineLayoutUnit& operator*=(FloatLineLayoutUnit& a, float b)
{
    a = a * b;
    return a;
}

inline FloatLineLayoutUnit& operator*=(FloatLineLayoutUnit& a, int b)
{
    a = a * b;
    return a;
}

inline FloatLineLayoutUnit& operator*=(FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    a = a * b;
    return a;
}

inline float& operator*=(float& a, const FloatLineLayoutUnit& b)
{
    a = a * b;
    return a;
}

inline int& operator*=(int& a, const FloatLineLayoutUnit& b)
{
    a = a * b;
    return a;
}

inline LayoutUnit& operator*=(LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a * b;
    return a;
}

inline FloatLineLayoutUnit& operator/=(FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a / b;
    return a;
}

inline FloatLineLayoutUnit& operator/=(FloatLineLayoutUnit& a, float b)
{
    a = a / b;
    return a;
}

inline FloatLineLayoutUnit& operator/=(FloatLineLayoutUnit& a, int b)
{
    a = a / b;
    return a;
}

inline FloatLineLayoutUnit& operator/=(FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    a = a / b;
    return a;
}

inline float& operator/=(float& a, const FloatLineLayoutUnit& b)
{
    a = a / b;
    return a;
}

inline int& operator/=(int& a, const FloatLineLayoutUnit& b)
{
    a = a / b;
    return a;
}

inline LayoutUnit& operator/=(LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a / b;
    return a;
}

inline FloatLineLayoutUnit& operator+=(FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a + b;
    return a;
}

inline FloatLineLayoutUnit& operator+=(FloatLineLayoutUnit& a, float b)
{
    a = a + b;
    return a;
}

inline FloatLineLayoutUnit& operator+=(FloatLineLayoutUnit& a, int b)
{
    a = a + b;
    return a;
}

inline FloatLineLayoutUnit& operator+=(FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    a = a + b;
    return a;
}

inline float& operator+=(float& a, const FloatLineLayoutUnit& b)
{
    a = a + b;
    return a;
}

inline int& operator+=(int& a, const FloatLineLayoutUnit& b)
{
    a = a + b;
    return a;
}

inline LayoutUnit& operator+=(LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a + b;
    return a;
}

inline FloatLineLayoutUnit& operator-=(FloatLineLayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a - b;
    return a;
}

inline FloatLineLayoutUnit& operator-=(FloatLineLayoutUnit& a, float b)
{
    a = a - b;
    return a;
}

inline FloatLineLayoutUnit& operator-=(FloatLineLayoutUnit& a, int b)
{
    a = a - b;
    return a;
}

inline FloatLineLayoutUnit& operator-=(FloatLineLayoutUnit& a, const LayoutUnit& b)
{
    a = a - b;
    return a;
}

inline float& operator-=(float& a, const FloatLineLayoutUnit& b)
{
    a = a - b;
    return a;
}

inline int& operator-=(int& a, const FloatLineLayoutUnit& b)
{
    a = a - b;
    return a;
}

inline LayoutUnit& operator-=(LayoutUnit& a, const FloatLineLayoutUnit& b)
{
    a = a - b;
    return a;
}

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

private:
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
