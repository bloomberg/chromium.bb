// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/IdleDeadline.h"

#include "wtf/CurrentTime.h"

namespace blink {

IdleDeadline::IdleDeadline(double deadlineSeconds, CallbackType callbackType)
    : m_deadlineSeconds(deadlineSeconds)
    , m_callbackType(callbackType)
{
}

double IdleDeadline::timeRemaining() const
{
    double timeRemaining = m_deadlineSeconds - monotonicallyIncreasingTime();
    if (timeRemaining < 0)
        timeRemaining = 0;

    return timeRemaining * 1000;
}

} // namespace blink
