// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForwardCodePointStateMachine_h
#define ForwardCodePointStateMachine_h

#include "core/CoreExport.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/Unicode.h"

namespace blink {

class CORE_EXPORT ForwardCodePointStateMachine {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ForwardCodePointStateMachine);

 public:
  ForwardCodePointStateMachine();
  ~ForwardCodePointStateMachine() = default;

  // Prepares by feeding preceding text.
  TextSegmentationMachineState feedPrecedingCodeUnit(UChar codeUnit);

  // Finds boundary offset by feeding following text.
  TextSegmentationMachineState feedFollowingCodeUnit(UChar codeUnit);

  // Returns true if we are at code point boundary.
  bool atCodePointBoundary();

  // Returns the next boundary offset.
  int getBoundaryOffset();

  // Resets the internal state to the initial state.
  void reset();

 private:
  enum class ForwardCodePointState;

  // The number of code units to be deleted.
  // Nothing to delete if there is an invalid surrogate pair.
  int m_codeUnitsToBeDeleted = 0;

  // The internal state.
  ForwardCodePointState m_state;
};

}  // namespace blink

#endif
