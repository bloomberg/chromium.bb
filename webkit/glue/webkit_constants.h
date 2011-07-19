// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBKIT_CONSTANTS_H_
#define WEBKIT_GLUE_WEBKIT_CONSTANTS_H_

namespace webkit_glue {

// Chromium sets the minimum interval timeout to 4ms, overriding the
// default of 10ms.  We'd like to go lower, however there are poorly
// coded websites out there which do create CPU-spinning loops.  Using
// 4ms prevents the CPU from spinning too busily and provides a balance
// between CPU spinning and the smallest possible interval timer.
const double kForegroundTabTimerInterval = 0.004;

// Provides control over the minimum timer interval for background tabs.
const double kBackgroundTabTimerInterval = 1.0;

} // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBKIT_CONSTANTS_H_
