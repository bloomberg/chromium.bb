// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBeginFrameArgs_h
#define WebBeginFrameArgs_h

namespace blink {

struct WebBeginFrameArgs {
    WebBeginFrameArgs(double lastFrameTimeMonotonic)
        : lastFrameTimeMonotonic(lastFrameTimeMonotonic)
    { }

    // FIXME: Upgrade the time in CLOCK_MONOTONIC values to use a TimeTick like
    // class rather than a bare double.

    // FIXME: Extend this class to include the fields from Chrome
    // BeginFrameArgs structure.

    // Time in CLOCK_MONOTONIC that is the most recent vsync time.
    double lastFrameTimeMonotonic;
};

} // namespace blink

#endif
