// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/Unicode.h"

#include <string>
#include <vector>

namespace blink {

class BackwardGraphemeBoundaryStateMachine;
class ForwardGraphemeBoundaryStateMachine;

// Processes the |machine| with preceding/following code points.
// The result string represents the output sequence of the state machine.
// Each character represents returned state of each action.
// I : Invalid
// R : NeedMoreCodeUnit (Repeat)
// S : NeedFollowingCodeUnit (Switch)
// F : Finished
//
// For example, if a state machine returns following sequence:
//   NeedMoreCodeUnit, NeedFollowingCodeUnit, Finished
// the returned string will be "RSF".
std::string processSequenceBackward(
    BackwardGraphemeBoundaryStateMachine*,
    const std::vector<UChar32>& preceding);

std::string processSequenceForward(
    ForwardGraphemeBoundaryStateMachine*,
    const std::vector<UChar32>& preceding,
    const std::vector<UChar32>& following);
} // namespace blink
