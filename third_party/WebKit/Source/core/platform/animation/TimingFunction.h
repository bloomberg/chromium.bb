/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef TimingFunction_h
#define TimingFunction_h

#include "RuntimeEnabledFeatures.h"
#include "core/platform/animation/AnimationUtilities.h" // For blend()
#include "core/platform/graphics/UnitBezier.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace WebCore {

class TimingFunction : public RefCounted<TimingFunction> {
public:

    enum Type {
        LinearFunction, CubicBezierFunction, StepsFunction, ChainedFunction
    };

    virtual ~TimingFunction() { }

    Type type() const { return m_type; }

    // Evaluates the timing function at the given fraction. The accuracy parameter provides a hint as to the required
    // accuracy and is not guaranteed.
    virtual double evaluate(double fraction, double accuracy) const = 0;
    virtual bool operator==(const TimingFunction& other) const = 0;

protected:
    TimingFunction(Type type)
        : m_type(type)
    {
    }

private:
    Type m_type;
};

class LinearTimingFunction : public TimingFunction {
public:
    static PassRefPtr<LinearTimingFunction> create()
    {
        return adoptRef(new LinearTimingFunction);
    }

    ~LinearTimingFunction() { }

    virtual double evaluate(double fraction, double) const
    {
        ASSERT(RuntimeEnabledFeatures::webAnimationsEnabled() || (fraction >= 0 && fraction <= 1));
        RELEASE_ASSERT_WITH_MESSAGE(!RuntimeEnabledFeatures::webAnimationsEnabled() || (fraction >= 0 && fraction <= 1), "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
        return fraction;
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        return other.type() == LinearFunction;
    }

private:
    LinearTimingFunction()
        : TimingFunction(LinearFunction)
    {
    }
};

class CubicBezierTimingFunction : public TimingFunction {
public:
    enum SubType {
        Ease,
        EaseIn,
        EaseOut,
        EaseInOut,
        Custom
    };

    static PassRefPtr<CubicBezierTimingFunction> create(double x1, double y1, double x2, double y2)
    {
        return adoptRef(new CubicBezierTimingFunction(Custom, x1, y1, x2, y2));
    }

    static CubicBezierTimingFunction* preset(SubType subType)
    {
        switch (subType) {
        case Ease:
            {
                static CubicBezierTimingFunction* ease = adoptRef(new CubicBezierTimingFunction(Ease, 0.25, 0.1, 0.25, 1.0)).leakRef();
                return ease;
            }
        case EaseIn:
            {
                static CubicBezierTimingFunction* easeIn = adoptRef(new CubicBezierTimingFunction(EaseIn, 0.42, 0.0, 1.0, 1.0)).leakRef();
                return easeIn;
            }
        case EaseOut:
            {
                static CubicBezierTimingFunction* easeOut = adoptRef(new CubicBezierTimingFunction(EaseOut, 0.0, 0.0, 0.58, 1.0)).leakRef();
                return easeOut;
            }
        case EaseInOut:
            {
                static CubicBezierTimingFunction* easeInOut = adoptRef(new CubicBezierTimingFunction(EaseInOut, 0.42, 0.0, 0.58, 1.0)).leakRef();
                return easeInOut;
            }
        default:
            ASSERT_NOT_REACHED();
            return 0;
        }
    }

    ~CubicBezierTimingFunction() { }

    virtual double evaluate(double fraction, double accuracy) const
    {
        ASSERT(RuntimeEnabledFeatures::webAnimationsEnabled() || (fraction >= 0 && fraction <= 1));
        RELEASE_ASSERT_WITH_MESSAGE(!RuntimeEnabledFeatures::webAnimationsEnabled() || (fraction >= 0 && fraction <= 1), "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
        if (!m_bezier)
            m_bezier = adoptPtr(new UnitBezier(m_x1, m_y1, m_x2, m_y2));
        return m_bezier->solve(fraction, accuracy);
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        if (other.type() == CubicBezierFunction) {
            const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(&other);
            if (m_subType != Custom)
                return m_subType == ctf->m_subType;

            return m_x1 == ctf->m_x1 && m_y1 == ctf->m_y1 && m_x2 == ctf->m_x2 && m_y2 == ctf->m_y2;
        }
        return false;
    }

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    SubType subType() const { return m_subType; }

private:
    explicit CubicBezierTimingFunction(SubType subType, double x1, double y1, double x2, double y2)
        : TimingFunction(CubicBezierFunction)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
        , m_subType(subType)
    {
    }

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
    SubType m_subType;
    mutable OwnPtr<UnitBezier> m_bezier;
};

class StepsTimingFunction : public TimingFunction {
public:
    enum SubType {
        Start,
        End,
        Custom
    };

    static PassRefPtr<StepsTimingFunction> create(int steps, bool stepAtStart)
    {
        return adoptRef(new StepsTimingFunction(Custom, steps, stepAtStart));
    }

    static StepsTimingFunction* preset(SubType subType)
    {
        switch (subType) {
        case Start:
            {
                static StepsTimingFunction* start = adoptRef(new StepsTimingFunction(Start, 1, true)).leakRef();
                return start;
            }
        case End:
            {
                static StepsTimingFunction* end = adoptRef(new StepsTimingFunction(End, 1, false)).leakRef();
                return end;
            }
        default:
            ASSERT_NOT_REACHED();
            return 0;
        }
    }


    ~StepsTimingFunction() { }

    virtual double evaluate(double fraction, double) const
    {
        ASSERT(RuntimeEnabledFeatures::webAnimationsEnabled() || (fraction >= 0 && fraction <= 1));
        RELEASE_ASSERT_WITH_MESSAGE(!RuntimeEnabledFeatures::webAnimationsEnabled() || (fraction >= 0 && fraction <= 1), "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
        return std::min(1.0, (floor(m_steps * fraction) + m_stepAtStart) / m_steps);
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        if (other.type() == StepsFunction) {
            const StepsTimingFunction* stf = static_cast<const StepsTimingFunction*>(&other);
            if (m_subType != Custom)
                return m_subType == stf->m_subType;
            return m_steps == stf->m_steps && m_stepAtStart == stf->m_stepAtStart;
        }
        return false;
    }

    int numberOfSteps() const { return m_steps; }
    bool stepAtStart() const { return m_stepAtStart; }

    SubType subType() const { return m_subType; }

private:
    StepsTimingFunction(SubType subType, int steps, bool stepAtStart)
        : TimingFunction(StepsFunction)
        , m_steps(steps)
        , m_stepAtStart(stepAtStart)
        , m_subType(subType)
    {
    }

    int m_steps;
    bool m_stepAtStart;
    SubType m_subType;
};

class ChainedTimingFunction : public TimingFunction {
public:
    static PassRefPtr<ChainedTimingFunction> create()
    {
        return adoptRef(new ChainedTimingFunction);
    }

    void appendSegment(double upperBound, TimingFunction* timingFunction)
    {
        double max = m_segments.isEmpty() ? 0 : m_segments.last().max();
        ASSERT(upperBound > max);
        m_segments.append(Segment(max, upperBound, timingFunction));
    }
    virtual double evaluate(double fraction, double accuracy) const
    {
        RELEASE_ASSERT_WITH_MESSAGE(fraction >= 0 && fraction <= 1, "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
        ASSERT(!m_segments.isEmpty());
        ASSERT(m_segments.last().max() == 1);
        const Segment* segment;
        for (size_t i = 0; i < m_segments.size(); ++i) {
            segment = &m_segments[i];
            if (fraction < segment->max())
                break;
        }
        return segment->evaluate(fraction, accuracy);
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        // This class is not exposed to CSS, so this method is not required.
        ASSERT_NOT_REACHED();
        return false;
    }

private:
    class Segment {
    public:
        Segment(double min, double max, TimingFunction* timingFunction)
            : m_min(min)
            , m_max(max)
            , m_timingFunction(timingFunction)
        { }

        double max() const { return m_max; }
        double evaluate(double fraction, double accuracy) const
        {
            return scaleFromLocal(m_timingFunction->evaluate(scaleToLocal(fraction), accuracy));
        }

    private:
        double scaleToLocal(double x) const { return (x - m_min) / (m_max - m_min); }
        double scaleFromLocal(double x) const { return blend(m_min, m_max, x); }

        double m_min;
        double m_max;
        RefPtr<TimingFunction> m_timingFunction;
    };

    ChainedTimingFunction()
        : TimingFunction(ChainedFunction)
    {
        ASSERT(RuntimeEnabledFeatures::webAnimationsEnabled());
    }

    Vector<Segment> m_segments;
};

} // namespace WebCore

#endif // TimingFunction_h
