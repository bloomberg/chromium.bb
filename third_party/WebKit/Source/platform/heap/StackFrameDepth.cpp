// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/StackFrameDepth.h"

#include "public/platform/Platform.h"
#include "wtf/StackUtil.h"

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

namespace blink {

static const char* s_avoidOptimization = nullptr;

// NEVER_INLINE ensures that |dummy| array on configureLimit() is not optimized
// away, and the stack frame base register is adjusted |kSafeStackFrameSize|.
NEVER_INLINE static uintptr_t currentStackFrameBaseOnCallee(const char* dummy) {
  s_avoidOptimization = dummy;
  return StackFrameDepth::currentStackFrame();
}

uintptr_t StackFrameDepth::getFallbackStackLimit() {
  // Allocate an |kSafeStackFrameSize|-sized object on stack and query
  // stack frame base after it.
  char dummy[kSafeStackFrameSize];

  // Check that the stack frame can be used.
  dummy[sizeof(dummy) - 1] = 0;
  return currentStackFrameBaseOnCallee(dummy);
}

void StackFrameDepth::enableStackLimit() {
  // All supported platforms will currently return a non-zero estimate,
  // except if ASan is enabled.
  size_t stackSize = WTF::getUnderestimatedStackSize();
  if (!stackSize) {
    m_stackFrameLimit = getFallbackStackLimit();
    return;
  }

  static const int kStackRoomSize = 1024;

  Address stackBase = reinterpret_cast<Address>(WTF::getStackStart());
  RELEASE_ASSERT(stackSize > static_cast<const size_t>(kStackRoomSize));
  size_t stackRoom = stackSize - kStackRoomSize;
  RELEASE_ASSERT(stackBase > reinterpret_cast<Address>(stackRoom));
  m_stackFrameLimit = reinterpret_cast<uintptr_t>(stackBase - stackRoom);

  // If current stack use is already exceeding estimated limit, mark as
  // disabled.
  if (!isSafeToRecurse())
    disableStackLimit();
}

}  // namespace blink
