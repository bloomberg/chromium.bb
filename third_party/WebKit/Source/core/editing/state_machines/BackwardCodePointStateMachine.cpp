// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackwardCodePointStateMachine.h"

namespace blink {

enum class BackwardCodePointStateMachine::BackwardCodePointState {
  NotSurrogate,
  TrailSurrogate,
  Invalid,
};

BackwardCodePointStateMachine::BackwardCodePointStateMachine()
    : m_state(BackwardCodePointState::NotSurrogate) {}

TextSegmentationMachineState
BackwardCodePointStateMachine::feedPrecedingCodeUnit(UChar codeUnit) {
  switch (m_state) {
    case BackwardCodePointState::NotSurrogate:
      if (U16_IS_LEAD(codeUnit)) {
        m_codeUnitsToBeDeleted = 0;
        m_state = BackwardCodePointState::Invalid;
        return TextSegmentationMachineState::Invalid;
      }
      ++m_codeUnitsToBeDeleted;
      if (U16_IS_TRAIL(codeUnit)) {
        m_state = BackwardCodePointState::TrailSurrogate;
        return TextSegmentationMachineState::NeedMoreCodeUnit;
      }
      return TextSegmentationMachineState::Finished;
    case BackwardCodePointState::TrailSurrogate:
      if (U16_IS_LEAD(codeUnit)) {
        ++m_codeUnitsToBeDeleted;
        m_state = BackwardCodePointState::NotSurrogate;
        return TextSegmentationMachineState::Finished;
      }
      m_codeUnitsToBeDeleted = 0;
      m_state = BackwardCodePointState::Invalid;
      return TextSegmentationMachineState::Invalid;
    case BackwardCodePointState::Invalid:
      m_codeUnitsToBeDeleted = 0;
      return TextSegmentationMachineState::Invalid;
  }
  NOTREACHED();
  return TextSegmentationMachineState::Invalid;
}

TextSegmentationMachineState
BackwardCodePointStateMachine::feedFollowingCodeUnit(UChar codeUnit) {
  NOTREACHED();
  return TextSegmentationMachineState::Invalid;
}

bool BackwardCodePointStateMachine::atCodePointBoundary() {
  return m_state == BackwardCodePointState::NotSurrogate;
}

int BackwardCodePointStateMachine::getBoundaryOffset() {
  return -m_codeUnitsToBeDeleted;
}

void BackwardCodePointStateMachine::reset() {
  m_codeUnitsToBeDeleted = 0;
  m_state = BackwardCodePointState::NotSurrogate;
}

}  // namespace blink
