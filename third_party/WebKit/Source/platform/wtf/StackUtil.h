// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StackUtil_h
#define StackUtil_h

#include "platform/wtf/Compiler.h"
#include "platform/wtf/WTFExport.h"
#include "platform/wtf/build_config.h"
#include <stddef.h>
#include <stdint.h>

namespace WTF {

WTF_EXPORT size_t getUnderestimatedStackSize();
WTF_EXPORT void* getStackStart();

namespace internal {

WTF_EXPORT extern uintptr_t s_mainThreadStackStart;
WTF_EXPORT extern uintptr_t s_mainThreadUnderestimatedStackSize;

WTF_EXPORT void initializeMainThreadStackEstimate();

#if OS(WIN) && COMPILER(MSVC)
size_t threadStackSize();
#endif

}  // namespace internal

// Returns true if the function is not called on the main thread. Note carefully
// that this function may have false positives, i.e. it can return true even if
// we are on the main thread. If the function returns false, we are certainly
// on the main thread.
inline bool mayNotBeMainThread() {
  uintptr_t dummy;
  uintptr_t addressDiff =
      internal::s_mainThreadStackStart - reinterpret_cast<uintptr_t>(&dummy);
  // This is a fast way to judge if we are in the main thread.
  // If |&dummy| is within |s_mainThreadUnderestimatedStackSize| byte from
  // the stack start of the main thread, we judge that we are in
  // the main thread.
  return addressDiff >= internal::s_mainThreadUnderestimatedStackSize;
}

}  // namespace WTF

#endif  // StackUtil_h
