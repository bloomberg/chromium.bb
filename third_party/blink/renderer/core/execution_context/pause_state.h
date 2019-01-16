// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_PAUSE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_PAUSE_STATE_H_

namespace blink {

// This enum represents the pausing state of the ExecutionContext.

enum class PauseState {
  // Pause tasks only. Used for nested event loops (alert, print).
  kPaused,
  // Freeze everything including media. Used for policies that
  // have auto stopping of media that is hidden.
  kFrozen
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_PAUSE_STATE_H_
