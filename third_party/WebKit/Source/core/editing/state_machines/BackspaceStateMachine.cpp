// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackspaceStateMachine.h"

#include "wtf/text/Unicode.h"

namespace blink {

namespace {
const TextSegmentationMachineState kInvalid = TextSegmentationMachineState::Invalid;
const TextSegmentationMachineState kNeedMoreCodeUnit = TextSegmentationMachineState::NeedMoreCodeUnit;
const TextSegmentationMachineState kFinished = TextSegmentationMachineState::Finished;
} // namespace

TextSegmentationMachineState BackspaceStateMachine::feedPrecedingCodeUnit(UChar codeUnit)
{
    uint32_t codePoint = codeUnit;
    if (U16_IS_LEAD(codeUnit)) {
        if (m_trailSurrogate == 0) {
            // Unpaired lead surrogate. Aborting with deleting broken surrogate.
            ++m_codeUnitsToBeDeleted;
            return kFinished;
        }
        codePoint = U16_GET_SUPPLEMENTARY(codeUnit, m_trailSurrogate);
        m_trailSurrogate = 0;
    } else if (U16_IS_TRAIL(codeUnit)) {
        if (m_trailSurrogate != 0) {
            // Unpaired trail surrogate. Aborting with deleting broken
            // surrogate.
            return kFinished;
        }
        m_trailSurrogate = codeUnit;
        return kNeedMoreCodeUnit;
    } else {
        if (m_trailSurrogate != 0) {
            // Unpaired trail surrogate. Aborting with deleting broken
            // surrogate.
            return kFinished;
        }
    }

    // TODO(nona): Handle emoji sequences.
    m_codeUnitsToBeDeleted = U16_LENGTH(codePoint);
    return kFinished;
}

TextSegmentationMachineState BackspaceStateMachine::tellEndOfPrecedingText()
{
    if (m_trailSurrogate != 0) {
        // Unpaired trail surrogate. Removing broken surrogate.
        ++m_codeUnitsToBeDeleted;
        m_trailSurrogate = 0;
    }
    return kFinished;
}

TextSegmentationMachineState BackspaceStateMachine::feedFollowingCodeUnit(UChar codeUnit)
{
    NOTREACHED();
    return kInvalid;
}

int BackspaceStateMachine::finalizeAndGetBoundaryOffset()
{
    if (m_trailSurrogate != 0) {
        // Unpaired trail surrogate. Removing broken surrogate.
        ++m_codeUnitsToBeDeleted;
        m_trailSurrogate = 0;
    }
    return -m_codeUnitsToBeDeleted;
}

void BackspaceStateMachine::reset()
{
    m_codeUnitsToBeDeleted = 0;
    m_trailSurrogate = 0;
}

} // namespace blink
