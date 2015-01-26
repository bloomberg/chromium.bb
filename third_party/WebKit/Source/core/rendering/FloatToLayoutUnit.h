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
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

class IntPoint;

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

    int round() const
    {
        return lroundf(m_value);
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

class FloatPointLineLayoutPoint {
public:
    FloatPointLineLayoutPoint() { }
    explicit FloatPointLineLayoutPoint(const FloatPoint& floatPoint) : m_value(floatPoint) { }
    FloatPointLineLayoutPoint(FloatLineLayoutUnit x, FloatLineLayoutUnit y) : m_value(x.toFloat(), y.toFloat()) { }
    FloatPointLineLayoutPoint(const IntPoint& intPoint) : m_value(intPoint) { }
    FloatPointLineLayoutPoint(const LayoutPoint& layoutPoint) : m_value(layoutPoint) { }

    operator LayoutPoint() const { return toLayoutPoint(); }

    FloatPoint toFloatPoint() const
    {
        return m_value;
    }

    LayoutPoint toLayoutPoint() const
    {
        return LayoutPoint(m_value);
    }

    LayoutPoint roundedLayoutPoint() const
    {
        return ::blink::roundedLayoutPoint(m_value);
    }

    LayoutPoint flooredLayoutPoint() const
    {
        return ::blink::flooredLayoutPoint(m_value);
    }

    FloatLineLayoutUnit x() const { return FloatLineLayoutUnit(m_value.x()); }
    FloatLineLayoutUnit y() const { return FloatLineLayoutUnit(m_value.y()); }

    void setX(FloatLineLayoutUnit x) { m_value.setX(x.toFloat()); }
    void setY(FloatLineLayoutUnit y) { m_value.setY(y.toFloat()); }
    void set(FloatLineLayoutUnit x, FloatLineLayoutUnit y) { m_value.set(x.toFloat(), y.toFloat()); }

    void move(FloatLineLayoutUnit dx, FloatLineLayoutUnit dy) { m_value.move(dx.toFloat(), dy.toFloat()); }
    void moveBy(const LayoutPoint& layoutPoint) { m_value.moveBy(layoutPoint); }

    void scale(float sx, float sy) { m_value.scale(sx, sy); }

private:
    FloatPoint m_value;
};

class FloatSizeLineLayoutSize {
public:
    FloatSizeLineLayoutSize() { }
    explicit FloatSizeLineLayoutSize(const FloatSize& floatSize) : m_value(floatSize) { }
    FloatSizeLineLayoutSize(FloatLineLayoutUnit width, FloatLineLayoutUnit height) : m_value(width.toFloat(), height.toFloat()) { }
    explicit FloatSizeLineLayoutSize(const IntSize& size) : m_value(size) { }
    FloatSizeLineLayoutSize(const LayoutSize& size) : m_value(size) { }

    operator LayoutSize() const { return toLayoutSize(); }

    FloatSize toFloatSize() const
    {
        return m_value;
    }

    LayoutSize toLayoutSize() const
    {
        return LayoutSize(m_value);
    }

    FloatLineLayoutUnit width() const { return FloatLineLayoutUnit(m_value.width()); }
    FloatLineLayoutUnit height() const { return FloatLineLayoutUnit(m_value.height()); }

    void setWidth(FloatLineLayoutUnit width) { m_value.setWidth(width.toFloat()); }
    void setHeight(FloatLineLayoutUnit height) { m_value.setHeight(height.toFloat()); }

private:
    FloatSize m_value;
};

class FloatRectLineLayoutRect {
public:
    FloatRectLineLayoutRect() { }
    FloatRectLineLayoutRect(const FloatRect& floatRect) : m_value(floatRect) { }
    FloatRectLineLayoutRect(const FloatPointLineLayoutPoint& location, const FloatSizeLineLayoutSize& size)
        : m_value(location.toFloatPoint(), size.toFloatSize()) { }
    FloatRectLineLayoutRect(FloatLineLayoutUnit x, FloatLineLayoutUnit y, FloatLineLayoutUnit width, FloatLineLayoutUnit height)
        : m_value(x.toFloat(), y.toFloat(), width.toFloat(), height.toFloat()) { }
    explicit FloatRectLineLayoutRect(const IntRect& rect) : m_value(rect) { }
    FloatRectLineLayoutRect(const LayoutRect& rect) : m_value(rect) { }

    operator LayoutRect() const { return toLayoutRect(); }

    FloatRect rawValue() const
    {
        return m_value;
    }


    FloatRect toFloatRect() const
    {
        return m_value;
    }

    LayoutRect toLayoutRect() const
    {
        return LayoutRect(m_value);
    }

    LayoutRect enclosingLayoutRect() const
    {
        return ::blink::enclosingLayoutRect(m_value);
    }

    FloatPointLineLayoutPoint location() const { return FloatPointLineLayoutPoint(m_value.location()); }
    FloatSizeLineLayoutSize size() const { return FloatSizeLineLayoutSize(m_value.size()); }

    void setLocation(const FloatPointLineLayoutPoint& location) { m_value.setLocation(location.toFloatPoint()); }
    void setSize(const FloatSizeLineLayoutSize& size) { m_value.setSize(size.toFloatSize()); }

    FloatLineLayoutUnit x() const { return FloatLineLayoutUnit(m_value.x()); }
    FloatLineLayoutUnit y() const { return FloatLineLayoutUnit(m_value.y()); }
    FloatLineLayoutUnit maxX() const { return FloatLineLayoutUnit(m_value.maxX()); }
    FloatLineLayoutUnit maxY() const { return FloatLineLayoutUnit(m_value.maxY()); }
    FloatLineLayoutUnit width() const { return FloatLineLayoutUnit(m_value.width()); }
    FloatLineLayoutUnit height() const { return FloatLineLayoutUnit(m_value.height()); }

    void setX(FloatLineLayoutUnit x) { m_value.setX(x.toFloat()); }
    void setY(FloatLineLayoutUnit y) { m_value.setY(y.toFloat()); }
    void setWidth(FloatLineLayoutUnit width) { m_value.setWidth(width.toFloat()); }
    void setHeight(FloatLineLayoutUnit height) { m_value.setHeight(height.toFloat()); }

    void unite(const FloatRectLineLayoutRect& rect) { m_value.unite(rect.toFloatRect()); }

    void scale(float s) { m_value.scale(s); }
    void scale(float sx, float sy) { m_value.scale(sx, sy); }

    bool intersects(const FloatRectLineLayoutRect& rect) const { return m_value.intersects(rect.toFloatRect()); }

private:
    FloatRect m_value;
};

#if ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)
using LineLayoutUnit = LayoutUnitLineLayoutUnit;
using LineLayoutPoint = LayoutPointLineLayoutPoint;
using LineLayoutRect = LayoutRectLineLayoutRect;
using LineLayoutSize = LayoutSizeLineLayoutSize;
#else // ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)
using LineLayoutUnit = FloatLineLayoutUnit;
using LineLayoutPoint = FloatPointLineLayoutPoint;
using LineLayoutRect = FloatRectLineLayoutRect;
using LineLayoutSize = FloatSizeLineLayoutSize;
#endif // ENABLE(LAYOUT_UNIT_IN_INLINE_BOXES)

using FloatWillBeLayoutUnit = LineLayoutUnit;
using FloatPointWillBeLayoutPoint = LineLayoutPoint;
using FloatRectWillBeLayoutRect = LineLayoutRect;
using FloatSizeWillBeLayoutSize = LineLayoutSize;

} // namespace blink

#endif // FloatToLayoutUnit_h
