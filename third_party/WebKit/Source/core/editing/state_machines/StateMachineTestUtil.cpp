// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/StateMachineTestUtil.h"

#include "core/editing/state_machines/BackwardGraphemeBoundaryStateMachine.h"
#include "core/editing/state_machines/ForwardGraphemeBoundaryStateMachine.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "wtf/Assertions.h"
#include <algorithm>
#include <vector>

namespace blink {

namespace {
char MachineStateToChar(TextSegmentationMachineState state)
{
    static const char indicators[] = {
        'I', // Invalid
        'R', // NeedMoreCodeUnit (Repeat)
        'S', // NeedFollowingCodeUnit (Switch)
        'F', // Finished
    };
    const auto& it = std::begin(indicators) + static_cast<size_t>(state);
    DCHECK_GE(it, std::begin(indicators)) << "Unknown backspace value";
    DCHECK_LT(it, std::end(indicators)) << "Unknown backspace value";
    return *it;
}

std::vector<UChar> codePointsToCodeUnits(const std::vector<UChar32>& codePoints)
{
    std::vector<UChar> out;
    for (const auto& codePoint : codePoints) {
        if (U16_LENGTH(codePoint) == 2) {
            out.push_back(U16_LEAD(codePoint));
            out.push_back(U16_TRAIL(codePoint));
        } else {
            out.push_back(static_cast<UChar>(codePoint));
        }
    }
    return out;
}

template<typename StateMachine>
std::string processSequence(StateMachine* machine,
    const std::vector<UChar32>& preceding,
    const std::vector<UChar32>& following)
{
    machine->reset();
    std::string out;
    TextSegmentationMachineState state = TextSegmentationMachineState::Invalid;
    std::vector<UChar> precedingCodeUnits = codePointsToCodeUnits(preceding);
    std::reverse(precedingCodeUnits.begin(), precedingCodeUnits.end());
    for (const auto& codeUnit : precedingCodeUnits) {
        state = machine->feedPrecedingCodeUnit(codeUnit);
        out += MachineStateToChar(state);
        switch (state) {
        case TextSegmentationMachineState::Invalid:
        case TextSegmentationMachineState::Finished:
            return out;
        case TextSegmentationMachineState::NeedMoreCodeUnit:
            continue;
        case TextSegmentationMachineState::NeedFollowingCodeUnit:
            break;
        }
    }
    if (preceding.empty()
        || state == TextSegmentationMachineState::NeedMoreCodeUnit) {
        state = machine->tellEndOfPrecedingText();
        out += MachineStateToChar(state);
    }
    if (state == TextSegmentationMachineState::Finished)
        return out;

    std::vector<UChar> followingCodeUnits = codePointsToCodeUnits(following);
    for (const auto& codeUnit : followingCodeUnits) {
        state = machine->feedFollowingCodeUnit(codeUnit);
        out += MachineStateToChar(state);
        switch (state) {
        case TextSegmentationMachineState::Invalid:
        case TextSegmentationMachineState::Finished:
            return out;
        case TextSegmentationMachineState::NeedMoreCodeUnit:
            continue;
        case TextSegmentationMachineState::NeedFollowingCodeUnit:
            break;
        }
    }
    return out;
}
} // namespace

std::string processSequenceBackward(
    BackwardGraphemeBoundaryStateMachine* machine,
    const std::vector<UChar32>& preceding)
{
    const std::string& out =
        processSequence(machine, preceding, std::vector<UChar32>());
    if (machine->finalizeAndGetBoundaryOffset()
        != machine->finalizeAndGetBoundaryOffset())
        return "State machine changes final offset after finished.";
    return out;
}

std::string processSequenceForward(
    ForwardGraphemeBoundaryStateMachine* machine,
    const std::vector<UChar32>& preceding,
    const std::vector<UChar32>& following)
{
    const std::string& out = processSequence(machine, preceding, following);
    if (machine->finalizeAndGetBoundaryOffset()
        != machine->finalizeAndGetBoundaryOffset())
        return "State machine changes final offset after finished.";
    return out;
}

} // namespace blink
