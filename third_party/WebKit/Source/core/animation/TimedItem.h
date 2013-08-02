/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TimedItem_h
#define TimedItem_h

#include "core/animation/Timing.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class Player;

static inline bool isNull(double value)
{
    return std::isnan(value);
}

static inline double nullValue()
{
    return std::numeric_limits<double>::quiet_NaN();
}

class TimedItemEventDelegate {
public:
    virtual ~TimedItemEventDelegate() { };
    virtual void onEventCondition(bool wasInPlay, bool isInPlay, double previousIteration, double currentIteration) = 0;
};

class TimedItem : public RefCounted<TimedItem> {
    friend class Player; // Calls attach/detach, updateInheritedTime.
public:
    virtual ~TimedItem() { }

    bool isCurrent() const { return ensureCalculated().isCurrent; }
    bool isInEffect() const { return ensureCalculated().isInEffect; }
    bool isInPlay() const { return ensureCalculated().isInPlay; }

    double startTime() const { return m_startTime; }

    double currentIteration() const { return ensureCalculated().currentIteration; }
    double activeDuration() const { return ensureCalculated().activeDuration; }
    double timeFraction() const { return ensureCalculated().timeFraction; }
    Player* player() const { return m_player; }

    enum Phase {
        PhaseBefore,
        PhaseActive,
        PhaseAfter,
        PhaseNone,
    };

protected:
    TimedItem(const Timing&, PassOwnPtr<TimedItemEventDelegate> = nullptr);

    // When TimedItem receives a new inherited time via updateInheritedTime
    // it will (if necessary) recalculate timings and (if necessary) call
    // updateChildrenAndEffects.
    void updateInheritedTime(double inheritedTime) const;
    virtual void updateChildrenAndEffects(bool wasInEffect) const = 0;
    virtual double intrinsicIterationDuration() const { return 0; };
    virtual void willDetach() = 0;

private:
    void attach(Player* player) { m_player = player; };
    void detach()
    {
        ASSERT(m_player);
        willDetach();
        m_player = 0;
    };

    // FIXME: m_parent and m_startTime are placeholders, they depend on timing groups.
    TimedItem* const m_parent;
    const double m_startTime;
    Player* m_player;
    Timing m_specified;
    OwnPtr<TimedItemEventDelegate> m_eventDelegate;

    // FIXME: Should be versioned by monotonic value on player.
    mutable struct CalculatedTiming {
        CalculatedTiming();
        double activeDuration;
        double currentIteration;
        double timeFraction;
        bool isCurrent;
        bool isInEffect;
        bool isInPlay;
    } m_calculated;

    // FIXME: Should check the version and reinherit time if inconsistent.
    const CalculatedTiming& ensureCalculated() const { return m_calculated; }
};

} // namespace WebCore

#endif
