// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackspaceStateMachine_h
#define BackspaceStateMachine_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/Unicode.h"

namespace blink {

class CORE_EXPORT BackspaceStateMachine {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(BackspaceStateMachine);
public:
    BackspaceStateMachine() = default;

    // Returns true when the state machine has stopped.
    bool updateState(UChar codeUnit);

    // Finalize the state machine and returns the code unit count to be deleted.
    // If the state machine hasn't finished, this method finishes the state
    // machine first.
    int finalizeAndGetCodeUnitCountToBeDeleted();

    // Resets the internal state to the initial state.
    void reset();

private:
    int m_codeUnitsToBeDeleted = 0;

    // Used for composing supplementary code point with surrogate pairs.
    UChar m_trailSurrogate = 0;
};

} // namespace blink

#endif
