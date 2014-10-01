// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DoublePoint_h
#define DoublePoint_h

#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntPoint.h"

namespace blink {

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
    explicit DoublePoint(const IntPoint& p)
        : m_x(p.x())
        , m_y(p.y())
    {
    }
    explicit DoublePoint(const FloatPoint& p)
        : m_x(p.x())
        , m_y(p.y())
    {
    }

    DoublePoint expandedTo(int x, int y) const
    {
        return DoublePoint(m_x > x ? m_x : x, m_y > y ? m_y : y);
    }

    DoublePoint shrunkTo(int x, int y) const
    {
        return DoublePoint(m_x < x ? m_x : x, m_y < y ? m_y : y);
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

inline DoubleSize operator-(const DoublePoint& a, const DoublePoint& b)
{
    return DoubleSize(a.x() - b.x(), a.y() - b.y());
}

inline IntPoint flooredIntPoint(const DoublePoint& p)
{
    return IntPoint(clampToInteger(floor(p.x())), clampToInteger(floor(p.y())));
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
