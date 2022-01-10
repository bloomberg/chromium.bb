// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_CHROME_UNWINDER_ANDROID_H_
#define BASE_PROFILER_CHROME_UNWINDER_ANDROID_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/profiler/unwinder.h"

#include "base/base_export.h"
#include "base/profiler/arm_cfi_table.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/register_context.h"

namespace base {

// Chrome unwinder implementation for Android, using ArmCfiTable.
class BASE_EXPORT ChromeUnwinderAndroid : public Unwinder {
 public:
  ChromeUnwinderAndroid(const ArmCFITable* cfi_table,
                        uintptr_t chrome_module_base_address);
  ~ChromeUnwinderAndroid() override;
  ChromeUnwinderAndroid(const ChromeUnwinderAndroid&) = delete;
  ChromeUnwinderAndroid& operator=(const ChromeUnwinderAndroid&) = delete;

  // Unwinder:
  bool CanUnwindFrom(const Frame& current_frame) const override;
  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) const override;

  static bool StepForTesting(RegisterContext* thread_context,
                             uintptr_t stack_top,
                             const ArmCFITable::FrameEntry& entry) {
    return Step(thread_context, stack_top, entry);
  }

 private:
  static bool Step(RegisterContext* thread_context,
                   uintptr_t stack_top,
                   const ArmCFITable::FrameEntry& entry);
  // Fallback setp that attempts to use lr as return address.
  static bool StepUsingLrRegister(RegisterContext* thread_context,
                                  uintptr_t stack_top);

  raw_ptr<const ArmCFITable> cfi_table_;
  const uintptr_t chrome_module_base_address_;
};

}  // namespace base

#endif  // BASE_PROFILER_CHROME_UNWINDER_ANDROID_H_
