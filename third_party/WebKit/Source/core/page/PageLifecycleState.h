// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PageLifecycleState_h
#define PageLifecycleState_h

namespace blink {

enum class PageLifecycleState {
  kUnknown,  // The page state is unknown.
  kActive,   // The page is visible and active.
  kHidden,   // The page is not visible but active.
  kStopped,  // The page is stopped.
};

}  // namespace blink
#endif  // PageLifecycleState_h
