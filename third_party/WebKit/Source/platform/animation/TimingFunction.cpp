// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/animation/TimingFunction.h"

namespace WebCore {

String LinearTimingFunction::toString() const
{
    return "linear";
}

double LinearTimingFunction::evaluate(double fraction, double) const
{
    ASSERT_WITH_MESSAGE(fraction >= 0 && fraction <= 1, "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
    return fraction;
}

String CubicBezierTimingFunction::toString() const
{
    switch (this->subType()) {
    case CubicBezierTimingFunction::Ease:
        return "ease";
    case CubicBezierTimingFunction::EaseIn:
        return "ease-in";
    case CubicBezierTimingFunction::EaseOut:
        return "ease-out";
    case CubicBezierTimingFunction::EaseInOut:
        return "ease-in-out";
    case CubicBezierTimingFunction::Custom:
        return "cubic-bezier(" + String::numberToStringECMAScript(this->x1()) + ", " +
            String::numberToStringECMAScript(this->y1()) + ", " + String::numberToStringECMAScript(this->x2()) +
            ", " + String::numberToStringECMAScript(this->y2()) + ")";
    default:
        ASSERT_NOT_REACHED();
    }
    return "";
}

double CubicBezierTimingFunction::evaluate(double fraction, double accuracy) const
{
    ASSERT_WITH_MESSAGE(fraction >= 0 && fraction <= 1, "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
    if (!m_bezier)
        m_bezier = adoptPtr(new UnitBezier(m_x1, m_y1, m_x2, m_y2));
    return m_bezier->solve(fraction, accuracy);
}

String StepsTimingFunction::toString() const
{
    StringBuilder builder;
    switch (this->subType()) {
    case StepsTimingFunction::Start:
        return "step-start";
    case StepsTimingFunction::End:
        return "step-end";
    case StepsTimingFunction::Custom:
        builder.append("steps(" + String::numberToStringECMAScript(this->numberOfSteps()) + ", ");
        builder.append(this->stepAtStart() ? "start" : "end");
        builder.append(")");
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return builder.toString();
}

double StepsTimingFunction::evaluate(double fraction, double) const
{
    ASSERT_WITH_MESSAGE(fraction >= 0 && fraction <= 1, "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
    return std::min(1.0, (floor(m_steps * fraction) + m_stepAtStart) / m_steps);
}

String ChainedTimingFunction::toString() const
{
    StringBuilder builder;
    builder.append("chained(");
    for (size_t i = 0; i < this->m_segments.size(); i++) {
        ChainedTimingFunction::Segment segment = this->m_segments[i];
        builder.append(segment.m_timingFunction->toString());
        builder.append("[" + String::numberToStringECMAScript(segment.m_min) + " -> " + String::numberToStringECMAScript(segment.m_max) + "]");
        if (i+1 != this->m_segments.size()) {
            builder.append(", ");
        }
    }
    builder.append(")");
    return builder.toString();
}

double ChainedTimingFunction::evaluate(double fraction, double accuracy) const
{
    ASSERT_WITH_MESSAGE(fraction >= 0 && fraction <= 1, "Web Animations not yet implemented: Timing function behavior outside the range [0, 1] is not yet specified");
    ASSERT(!m_segments.isEmpty());
    ASSERT(m_segments.last().max() == 1);
    size_t i = 0;
    const Segment* segment = &m_segments[i++];
    while (fraction >= segment->max() && i < m_segments.size()) {
        segment = &m_segments[i++];
    }
    return segment->evaluate(fraction, accuracy);
}

} // namespace WebCore
