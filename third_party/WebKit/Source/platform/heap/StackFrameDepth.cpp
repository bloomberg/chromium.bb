// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/StackFrameDepth.h"

#include "platform/wtf/StackUtil.h"
#include "public/platform/Platform.h"

#if defined(OS_WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

namespace blink {

static const char* g_avoid_optimization = nullptr;

// NEVER_INLINE ensures that |dummy| array on configureLimit() is not optimized
// away, and the stack frame base register is adjusted |kSafeStackFrameSize|.
NEVER_INLINE static uintptr_t CurrentStackFrameBaseOnCallee(const char* dummy) {
  g_avoid_optimization = dummy;
  return StackFrameDepth::CurrentStackFrame();
}

uintptr_t StackFrameDepth::GetFallbackStackLimit() {
  // Allocate an |kSafeStackFrameSize|-sized object on stack and query
  // stack frame base after it.
  char dummy[kSafeStackFrameSize];

  // Check that the stack frame can be used.
  dummy[sizeof(dummy) - 1] = 0;
  return CurrentStackFrameBaseOnCallee(dummy);
}

void StackFrameDepth::EnableStackLimit() {
  // All supported platforms will currently return a non-zero estimate,
  // except if ASan is enabled.
  size_t stack_size = WTF::GetUnderestimatedStackSize();
  if (!stack_size) {
    stack_frame_limit_ = GetFallbackStackLimit();
    return;
  }

  static const int kStackRoomSize = 1024;

  Address stack_base = reinterpret_cast<Address>(WTF::GetStackStart());
  CHECK_GT(stack_size, static_cast<const size_t>(kStackRoomSize));
  size_t stack_room = stack_size - kStackRoomSize;
  CHECK_GT(stack_base, reinterpret_cast<Address>(stack_room));
  stack_frame_limit_ = reinterpret_cast<uintptr_t>(stack_base - stack_room);

  // If current stack use is already exceeding estimated limit, mark as
  // disabled.
  if (!IsSafeToRecurse())
    DisableStackLimit();
}

}  // namespace blink
