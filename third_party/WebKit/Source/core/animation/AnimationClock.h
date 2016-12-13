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

#ifndef AnimationClock_h
#define AnimationClock_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/CurrentTime.h"
#include "wtf/Noncopyable.h"

#include <limits>

namespace blink {

// Maintains a stationary clock time during script execution.  Tries to track
// the glass time (the moment photons leave the screen) of the current animation
// frame.
class CORE_EXPORT AnimationClock {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(AnimationClock);

 public:
  explicit AnimationClock(WTF::TimeFunction monotonicallyIncreasingTime =
                              WTF::monotonicallyIncreasingTime)
      : m_monotonicallyIncreasingTime(monotonicallyIncreasingTime),
        m_time(0),
        m_taskForWhichTimeWasCalculated(std::numeric_limits<unsigned>::max()) {}

  void updateTime(double time);
  double currentTime();
  void resetTimeForTesting(double time = 0);

  // notifyTaskStart should be called right before the main message loop starts
  // to run the next task from the message queue.
  static void notifyTaskStart() { ++s_currentlyRunningTask; }

 private:
  WTF::TimeFunction m_monotonicallyIncreasingTime;
  double m_time;
  unsigned m_taskForWhichTimeWasCalculated;
  static unsigned s_currentlyRunningTask;
};

}  // namespace blink

#endif  // AnimationClock_h
