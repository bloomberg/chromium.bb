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

#include "platform/animation/AnimationUtilities.h" // For blend()
#include "platform/animation/UnitBezier.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT TimingFunction : public RefCounted<TimingFunction> {
public:

    enum FunctionType {
        kLinearFunction, kCubicBezierFunction, kStepsFunction
    };

    virtual ~TimingFunction() { }

    FunctionType type() const { return m_type; }

    virtual String toString() const = 0;

    // Evaluates the timing function at the given fraction. The accuracy parameter provides a hint as to the required
    // accuracy and is not guaranteed.
    virtual double evaluate(double fraction, double accuracy) const = 0;

    // This function returns the minimum and maximum values obtainable when
    // calling evaluate();
    virtual void range(double* minValue, double* maxValue) const = 0;

protected:
    TimingFunction(FunctionType type)
        : m_type(type)
    {
    }

private:
    FunctionType m_type;
};

class PLATFORM_EXPORT LinearTimingFunction final : public TimingFunction {
public:
    static LinearTimingFunction* shared()
    {
        DEFINE_STATIC_REF(LinearTimingFunction, linear, (adoptRef(new LinearTimingFunction())));
        return linear;
    }

    ~LinearTimingFunction() override { }

    String toString() const override;

    double evaluate(double fraction, double) const override;
    void range(double* minValue, double* maxValue) const override;
private:
    LinearTimingFunction()
        : TimingFunction(kLinearFunction)
    {
    }
};

class PLATFORM_EXPORT CubicBezierTimingFunction final : public TimingFunction {
public:
    enum FunctionSubType {
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

    static CubicBezierTimingFunction* preset(FunctionSubType subType)
    {
        switch (subType) {
        case Ease:
            {
                DEFINE_STATIC_REF(CubicBezierTimingFunction, ease, (adoptRef(new CubicBezierTimingFunction(Ease, 0.25, 0.1, 0.25, 1.0))));
                return ease;
            }
        case EaseIn:
            {
                DEFINE_STATIC_REF(CubicBezierTimingFunction, easeIn, (adoptRef(new CubicBezierTimingFunction(EaseIn, 0.42, 0.0, 1.0, 1.0))));
                return easeIn;
            }
        case EaseOut:
            {
                DEFINE_STATIC_REF(CubicBezierTimingFunction, easeOut, (adoptRef(new CubicBezierTimingFunction(EaseOut, 0.0, 0.0, 0.58, 1.0))));
                return easeOut;
            }
        case EaseInOut:
            {
                DEFINE_STATIC_REF(CubicBezierTimingFunction, easeInOut, (adoptRef(new CubicBezierTimingFunction(EaseInOut, 0.42, 0.0, 0.58, 1.0))));
                return easeInOut;
            }
        default:
            ASSERT_NOT_REACHED();
            return 0;
        }
    }

    ~CubicBezierTimingFunction() override { }

    String toString() const override;

    double evaluate(double fraction, double accuracy) const override;
    void range(double* minValue, double* maxValue) const override;

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    FunctionSubType subType() const { return m_subType; }

private:
    explicit CubicBezierTimingFunction(FunctionSubType subType, double x1, double y1, double x2, double y2)
        : TimingFunction(kCubicBezierFunction)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
        , m_subType(subType)
    {
    }

    // Finds points on the cubic bezier that cross the given horizontal
    // line, storing their x values in solution1-3 and returning the
    // number of solutions found.
    size_t findIntersections(double intersectionY, double& solution1, double& solution2, double& solution3) const;

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
    FunctionSubType m_subType;
    mutable OwnPtr<UnitBezier> m_bezier;
};

class PLATFORM_EXPORT StepsTimingFunction final : public TimingFunction {
public:
    enum StepAtPosition {
        Start,
        Middle,
        End
    };

    static PassRefPtr<StepsTimingFunction> create(int steps, StepAtPosition stepAtPosition)
    {
        return adoptRef(new StepsTimingFunction(steps, stepAtPosition));
    }

    static StepsTimingFunction* preset(StepAtPosition position)
    {
        DEFINE_STATIC_REF(StepsTimingFunction, start, create(1, Start));
        DEFINE_STATIC_REF(StepsTimingFunction, middle, create(1, Middle));
        DEFINE_STATIC_REF(StepsTimingFunction, end, create(1, End));
        switch (position) {
        case Start:
            return start;
        case Middle:
            return middle;
        case End:
            return end;
        default:
            ASSERT_NOT_REACHED();
            return end;
        }
    }


    ~StepsTimingFunction() override { }

    String toString() const override;

    double evaluate(double fraction, double) const override;
    void range(double* minValue, double* maxValue) const override;

    int numberOfSteps() const { return m_steps; }
    StepAtPosition getStepAtPosition() const { return m_stepAtPosition; }

private:
    StepsTimingFunction(int steps, StepAtPosition stepAtPosition)
        : TimingFunction(kStepsFunction)
        , m_steps(steps)
        , m_stepAtPosition(stepAtPosition)
    {
    }

    int m_steps;
    StepAtPosition m_stepAtPosition;
};

PLATFORM_EXPORT bool operator==(const LinearTimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator==(const CubicBezierTimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator==(const StepsTimingFunction&, const TimingFunction&);

PLATFORM_EXPORT bool operator==(const TimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator!=(const TimingFunction&, const TimingFunction&);

#define DEFINE_TIMING_FUNCTION_TYPE_CASTS(typeName) \
    DEFINE_TYPE_CASTS( \
        typeName##TimingFunction, TimingFunction, value, \
        value->type() == TimingFunction::k##typeName##Function, \
        value.type() == TimingFunction::k##typeName##Function)

DEFINE_TIMING_FUNCTION_TYPE_CASTS(Linear);
DEFINE_TIMING_FUNCTION_TYPE_CASTS(CubicBezier);
DEFINE_TIMING_FUNCTION_TYPE_CASTS(Steps);

} // namespace blink

#endif // TimingFunction_h
