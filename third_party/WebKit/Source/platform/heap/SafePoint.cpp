// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/SafePoint.h"

#include "platform/heap/Heap.h"

namespace blink {

using PushAllRegistersCallback = void (*)(SafePointBarrier*,
                                          ThreadState*,
                                          intptr_t*);
extern "C" void pushAllRegisters(SafePointBarrier*,
                                 ThreadState*,
                                 PushAllRegistersCallback);

SafePointBarrier::SafePointBarrier() {}

SafePointBarrier::~SafePointBarrier() {}

void SafePointBarrier::enterSafePoint(ThreadState* state) {
  ASSERT(!state->sweepForbidden());
  pushAllRegisters(this, state, enterSafePointAfterPushRegisters);
}

void SafePointBarrier::leaveSafePoint(ThreadState* state,
                                      SafePointAwareMutexLocker* locker) {
}

void SafePointBarrier::doEnterSafePoint(ThreadState* state,
                                        intptr_t* stackEnd) {
  state->recordStackEnd(stackEnd);
  state->copyStackUntilSafePointScope();
}

}  // namespace blink
