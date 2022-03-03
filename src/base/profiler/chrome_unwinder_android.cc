// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/chrome_unwinder_android.h"

#include "base/numerics/checked_math.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/native_unwinder.h"
#include "build/build_config.h"

namespace base {

ChromeUnwinderAndroid::ChromeUnwinderAndroid(
    const ArmCFITable* cfi_table,
    uintptr_t chrome_module_base_address)
    : cfi_table_(cfi_table),
      chrome_module_base_address_(chrome_module_base_address) {
  DCHECK(cfi_table_);
}

ChromeUnwinderAndroid::~ChromeUnwinderAndroid() = default;

bool ChromeUnwinderAndroid::CanUnwindFrom(const Frame& current_frame) const {
  return current_frame.module &&
         current_frame.module->GetBaseAddress() == chrome_module_base_address_;
}

UnwindResult ChromeUnwinderAndroid::TryUnwind(RegisterContext* thread_context,
                                              uintptr_t stack_top,
                                              std::vector<Frame>* stack) const {
  DCHECK(CanUnwindFrom(stack->back()));
  do {
    const ModuleCache::Module* module = stack->back().module;
    uintptr_t pc = RegisterContextInstructionPointer(thread_context);
    DCHECK_GE(pc, module->GetBaseAddress());
    uintptr_t func_addr = pc - module->GetBaseAddress();

    auto entry = cfi_table_->FindEntryForAddress(func_addr);
    if (entry) {
      if (!Step(thread_context, stack_top, *entry))
        return UnwindResult::kAborted;
    } else if (stack->size() == 1) {
      // Try unwinding by sourcing the return address from the lr register.
      if (!StepUsingLrRegister(thread_context, stack_top))
        return UnwindResult::kAborted;
    } else {
      return UnwindResult::kAborted;
    }
    stack->emplace_back(RegisterContextInstructionPointer(thread_context),
                        module_cache()->GetModuleForAddress(
                            RegisterContextInstructionPointer(thread_context)));
  } while (CanUnwindFrom(stack->back()));
  return UnwindResult::kUnrecognizedFrame;
}

// static
bool ChromeUnwinderAndroid::Step(RegisterContext* thread_context,
                                 uintptr_t stack_top,
                                 const ArmCFITable::FrameEntry& entry) {
  CHECK_NE(RegisterContextStackPointer(thread_context), 0U);
  CHECK_LE(RegisterContextStackPointer(thread_context), stack_top);
  if (entry.cfa_offset == 0)
    return StepUsingLrRegister(thread_context, stack_top);

  // The rules for unwinding using the CFI information are:
  // SP_prev = SP_cur + cfa_offset and
  // PC_prev = * (SP_prev - ra_offset).
  auto new_sp =
      CheckedNumeric<uintptr_t>(RegisterContextStackPointer(thread_context)) +
      CheckedNumeric<uint16_t>(entry.cfa_offset);
  if (!new_sp.AssignIfValid(&RegisterContextStackPointer(thread_context)) ||
      RegisterContextStackPointer(thread_context) >= stack_top) {
    return false;
  }

  if (entry.ra_offset > entry.cfa_offset)
    return false;

  // Underflow is prevented because |ra_offset| <= |cfa_offset|.
  uintptr_t ip_address = (new_sp - CheckedNumeric<uint16_t>(entry.ra_offset))
                             .ValueOrDie<uintptr_t>();
  RegisterContextInstructionPointer(thread_context) =
      *reinterpret_cast<uintptr_t*>(ip_address);
  return true;
}

// static
bool ChromeUnwinderAndroid::StepUsingLrRegister(RegisterContext* thread_context,
                                                uintptr_t stack_top) {
  CHECK_NE(RegisterContextStackPointer(thread_context), 0U);
  CHECK_LE(RegisterContextStackPointer(thread_context), stack_top);

  uintptr_t pc = RegisterContextInstructionPointer(thread_context);
  uintptr_t return_address = static_cast<uintptr_t>(thread_context->arm_lr);

  // The step failed if the pc doesn't change.
  if (pc == return_address)
    return false;

  RegisterContextInstructionPointer(thread_context) = return_address;
  return true;
}

}  // namespace base
