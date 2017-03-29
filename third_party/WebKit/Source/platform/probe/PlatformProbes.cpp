// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/probe/PlatformProbes.h"

namespace blink {
namespace probe {

double ProbeBase::captureStartTime() const {
  if (!m_startTime)
    m_startTime = monotonicallyIncreasingTime();
  return m_startTime;
}

double ProbeBase::captureEndTime() const {
  if (!m_endTime)
    m_endTime = monotonicallyIncreasingTime();
  return m_endTime;
}

double ProbeBase::duration() const {
  DCHECK(m_startTime);
  return captureEndTime() - m_startTime;
}

}  // namespace probe
}  // namespace blink
