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

#include "core/platform/graphics/UnitBezier.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include <algorithm>

namespace WebCore {

class TimingFunction : public RefCounted<TimingFunction> {
public:

    enum TimingFunctionType {
        LinearFunction, CubicBezierFunction, StepsFunction
    };

    virtual ~TimingFunction() { }

    TimingFunctionType type() const { return m_type; }

    bool isLinearTimingFunction() const { return m_type == LinearFunction; }
    bool isCubicBezierTimingFunction() const { return m_type == CubicBezierFunction; }
    bool isStepsTimingFunction() const { return m_type == StepsFunction; }

    // Evaluates the timing function at the given fraction. The accuracy parameter provides a hint as to the required
    // accuracy and is not guaranteed.
    virtual double evaluate(double fraction, double accuracy) const = 0;
    virtual bool operator==(const TimingFunction& other) const = 0;

protected:
    TimingFunction(TimingFunctionType type)
        : m_type(type)
    {
    }

    TimingFunctionType m_type;
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
        return fraction;
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        return other.isLinearTimingFunction();
    }

private:
    LinearTimingFunction()
        : TimingFunction(LinearFunction)
    {
    }
};

class CubicBezierTimingFunction : public TimingFunction {
public:
    enum TimingFunctionPreset {
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

    static CubicBezierTimingFunction* preset(TimingFunctionPreset type)
    {
        switch (type) {
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
        return UnitBezier(m_x1, m_y1, m_x2, m_y2).solve(fraction, accuracy);
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        if (other.isCubicBezierTimingFunction()) {
            const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(&other);
            if (m_timingFunctionPreset != Custom)
                return m_timingFunctionPreset == ctf->m_timingFunctionPreset;

            return m_x1 == ctf->m_x1 && m_y1 == ctf->m_y1 && m_x2 == ctf->m_x2 && m_y2 == ctf->m_y2;
        }
        return false;
    }

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    TimingFunctionPreset timingFunctionPreset() const { return m_timingFunctionPreset; }

private:
    explicit CubicBezierTimingFunction(TimingFunctionPreset preset, double x1, double y1, double x2, double y2)
        : TimingFunction(CubicBezierFunction)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
        , m_timingFunctionPreset(preset)
    {
    }

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
    TimingFunctionPreset m_timingFunctionPreset;
};

class StepsTimingFunction : public TimingFunction {
public:
    static PassRefPtr<StepsTimingFunction> create(int steps, bool stepAtStart)
    {
        return adoptRef(new StepsTimingFunction(steps, stepAtStart));
    }

    ~StepsTimingFunction() { }

    virtual double evaluate(double fraction, double) const
    {
        return std::min(1.0, (floor(m_steps * fraction) + m_stepAtStart) / m_steps);
    }

    virtual bool operator==(const TimingFunction& other) const
    {
        if (other.isStepsTimingFunction()) {
            const StepsTimingFunction* stf = static_cast<const StepsTimingFunction*>(&other);
            return m_steps == stf->m_steps && m_stepAtStart == stf->m_stepAtStart;
        }
        return false;
    }

    int numberOfSteps() const { return m_steps; }
    bool stepAtStart() const { return m_stepAtStart; }

private:
    StepsTimingFunction(int steps, bool stepAtStart)
        : TimingFunction(StepsFunction)
        , m_steps(steps)
        , m_stepAtStart(stepAtStart)
    {
    }

    int m_steps;
    bool m_stepAtStart;
};

} // namespace WebCore

#endif // TimingFunction_h
