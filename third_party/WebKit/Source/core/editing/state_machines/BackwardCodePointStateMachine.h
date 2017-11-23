// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackwardCodePointStateMachine_h
#define BackwardCodePointStateMachine_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/Unicode.h"

namespace blink {

class CORE_EXPORT BackwardCodePointStateMachine {
  STACK_ALLOCATED();

 public:
  BackwardCodePointStateMachine();
  ~BackwardCodePointStateMachine() = default;

  // Prepares by feeding preceding text.
  TextSegmentationMachineState FeedPrecedingCodeUnit(UChar code_unit);

  // Finds boundary offset by feeding following text.
  TextSegmentationMachineState FeedFollowingCodeUnit(UChar code_unit);

  // Returns true if we are at code point boundary.
  bool AtCodePointBoundary();

  // Returns the next boundary offset.
  int GetBoundaryOffset();

  // Resets the internal state to the initial state.
  void Reset();

 private:
  enum class BackwardCodePointState;

  // The number of code units to be deleted.
  // Nothing to delete if there is an invalid surrogate pair.
  int code_units_to_be_deleted_ = 0;

  // The internal state.
  BackwardCodePointState state_;

  DISALLOW_COPY_AND_ASSIGN(BackwardCodePointStateMachine);
};

}  // namespace blink

#endif
