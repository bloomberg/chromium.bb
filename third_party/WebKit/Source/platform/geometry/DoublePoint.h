// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DoublePoint_h
#define DoublePoint_h

#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"

namespace blink {

class LayoutPoint;

class PLATFORM_EXPORT DoublePoint {
public:
    DoublePoint()
        : m_x(0)
        , m_y(0)
    {
    }
    DoublePoint(double x, double y)
        : m_x(x)
        , m_y(y)
    {
    }
    DoublePoint(const IntPoint& p)
        : m_x(p.x())
        , m_y(p.y())
    {
    }
    DoublePoint(const FloatPoint& p)
        : m_x(p.x())
        , m_y(p.y())
    {
    }
    explicit DoublePoint(const LayoutPoint&);

    explicit DoublePoint(const IntSize& size)
        : m_x(size.width()), m_y(size.height())
    {
    }

    explicit DoublePoint(const FloatSize& size)
        : m_x(size.width()), m_y(size.height())
    {
    }

    explicit DoublePoint(const DoubleSize& size)
        : m_x(size.width()), m_y(size.height())
    {
    }

    DoublePoint expandedTo(const DoublePoint& other) const
    {
        return DoublePoint(std::max(m_x, other.m_x), std::max(m_y, other.m_y));
    }

    DoublePoint shrunkTo(const DoublePoint& other) const
    {
        return DoublePoint(std::min(m_x, other.m_x), std::min(m_y, other.m_y));
    }

    double x() const { return m_x; }
    double y() const { return m_y; }

private:
    double m_x, m_y;
};

inline bool operator==(const DoublePoint& a, const DoublePoint& b)
{
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const DoublePoint& a, const DoublePoint& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

inline DoublePoint operator+(const DoublePoint& a, const DoubleSize& b)
{
    return DoublePoint(a.x() + b.width(), a.y() + b.height());
}

inline DoublePoint operator+(const IntPoint& a, const DoubleSize& b)
{
    return DoublePoint(a.x() + b.width(), a.y() + b.height());
}

inline DoubleSize operator-(const DoublePoint& a, const DoublePoint& b)
{
    return DoubleSize(a.x() - b.x(), a.y() - b.y());
}

inline DoublePoint operator-(const DoublePoint& a)
{
    return DoublePoint(-a.x(), -a.y());
}

inline IntPoint flooredIntPoint(const DoublePoint& p)
{
    return IntPoint(clampTo<int>(floor(p.x())), clampTo<int>(floor(p.y())));
}

inline FloatPoint toFloatPoint(const DoublePoint& a)
{
    return FloatPoint(a.x(), a.y());
}

inline DoubleSize toDoubleSize(const DoublePoint& a)
{
    return DoubleSize(a.x(), a.y());
}

}

#endif
