// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/ForwardCodePointStateMachine.h"

namespace blink {

enum class ForwardCodePointStateMachine::ForwardCodePointState {
  NotSurrogate,
  LeadSurrogate,
  Invalid,
};

ForwardCodePointStateMachine::ForwardCodePointStateMachine()
    : m_state(ForwardCodePointState::NotSurrogate) {}

TextSegmentationMachineState
ForwardCodePointStateMachine::feedFollowingCodeUnit(UChar codeUnit) {
  switch (m_state) {
    case ForwardCodePointState::NotSurrogate:
      if (U16_IS_TRAIL(codeUnit)) {
        m_codeUnitsToBeDeleted = 0;
        m_state = ForwardCodePointState::Invalid;
        return TextSegmentationMachineState::Invalid;
      }
      ++m_codeUnitsToBeDeleted;
      if (U16_IS_LEAD(codeUnit)) {
        m_state = ForwardCodePointState::LeadSurrogate;
        return TextSegmentationMachineState::NeedMoreCodeUnit;
      }
      return TextSegmentationMachineState::Finished;
    case ForwardCodePointState::LeadSurrogate:
      if (U16_IS_TRAIL(codeUnit)) {
        ++m_codeUnitsToBeDeleted;
        m_state = ForwardCodePointState::NotSurrogate;
        return TextSegmentationMachineState::Finished;
      }
      m_codeUnitsToBeDeleted = 0;
      m_state = ForwardCodePointState::Invalid;
      return TextSegmentationMachineState::Invalid;
    case ForwardCodePointState::Invalid:
      m_codeUnitsToBeDeleted = 0;
      return TextSegmentationMachineState::Invalid;
  }
  NOTREACHED();
  return TextSegmentationMachineState::Invalid;
}

TextSegmentationMachineState
ForwardCodePointStateMachine::feedPrecedingCodeUnit(UChar codeUnit) {
  NOTREACHED();
  return TextSegmentationMachineState::Invalid;
}

bool ForwardCodePointStateMachine::atCodePointBoundary() {
  return m_state == ForwardCodePointState::NotSurrogate;
}

int ForwardCodePointStateMachine::getBoundaryOffset() {
  return m_codeUnitsToBeDeleted;
}

void ForwardCodePointStateMachine::reset() {
  m_codeUnitsToBeDeleted = 0;
  m_state = ForwardCodePointState::NotSurrogate;
}

}  // namespace blink
