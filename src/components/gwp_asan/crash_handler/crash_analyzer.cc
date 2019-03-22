// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <algorithm>

#include <stddef.h>

#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"
#include "third_party/crashpad/crashpad/util/process/process_memory.h"

namespace gwp_asan {
namespace internal {

using GwpAsanCrashAnalysisResult = CrashAnalyzer::GwpAsanCrashAnalysisResult;

// TODO: Delete out-of-line constexpr defininitons once C++17 is in use.
constexpr size_t CrashAnalyzer::kMaxStackTraceLen;

GwpAsanCrashAnalysisResult CrashAnalyzer::GetExceptionInfo(
    const crashpad::ProcessSnapshot& process_snapshot,
    gwp_asan::Crash* proto) {
  crashpad::VMAddress gpa_ptr = GetAllocatorAddress(process_snapshot);
  // If the annotation wasn't present, GWP-ASan wasn't enabled.
  if (!gpa_ptr)
    return GwpAsanCrashAnalysisResult::kUnrelatedCrash;

  const crashpad::ExceptionSnapshot* exception = process_snapshot.Exception();
  if (!exception)
    return GwpAsanCrashAnalysisResult::kUnrelatedCrash;

  if (!exception->Context())
    return GwpAsanCrashAnalysisResult::kErrorNullCpuContext;

#if defined(ARCH_CPU_64_BITS)
  constexpr bool is_64_bit = true;
#else
  constexpr bool is_64_bit = false;
#endif

  // TODO(vtsyrklevich): Look at using crashpad's process_types to read the GPA
  // state bitness-independently.
  if (exception->Context()->Is64Bit() != is_64_bit)
    return GwpAsanCrashAnalysisResult::kErrorMismatchedBitness;

  crashpad::VMAddress crash_addr = GetAccessAddress(*exception);
  if (!crash_addr)
    return GwpAsanCrashAnalysisResult::kUnrelatedCrash;

  if (!process_snapshot.Memory())
    return GwpAsanCrashAnalysisResult::kErrorNullProcessMemory;

  return AnalyzeCrashedAllocator(*process_snapshot.Memory(), gpa_ptr,
                                 crash_addr, proto);
}

crashpad::VMAddress CrashAnalyzer::GetAllocatorAddress(
    const crashpad::ProcessSnapshot& process_snapshot) {
  for (auto* module : process_snapshot.Modules()) {
    for (auto annotation : module->AnnotationObjects()) {
      if (annotation.name != kGpaCrashKey)
        continue;

      if (annotation.type !=
          static_cast<uint16_t>(crashpad::Annotation::Type::kString)) {
        DLOG(ERROR) << "Bad annotation type " << annotation.type;
        return 0;
      }

      std::string annotation_str(reinterpret_cast<char*>(&annotation.value[0]),
                                 annotation.value.size());
      uint64_t value;
      if (!base::HexStringToUInt64(annotation_str, &value))
        return 0;
      return value;
    }
  }

  return 0;
}

GwpAsanCrashAnalysisResult CrashAnalyzer::AnalyzeCrashedAllocator(
    const crashpad::ProcessMemory& memory,
    crashpad::VMAddress gpa_addr,
    crashpad::VMAddress exception_addr,
    gwp_asan::Crash* proto) {
  AllocatorState unsafe_state;
  if (!memory.Read(gpa_addr, sizeof(unsafe_state), &unsafe_state)) {
    DLOG(ERROR) << "Failed to read AllocatorState from process.";
    return GwpAsanCrashAnalysisResult::kErrorFailedToReadAllocator;
  }

  if (!unsafe_state.IsValid()) {
    DLOG(ERROR) << "Allocator sanity check failed!";
    return GwpAsanCrashAnalysisResult::kErrorAllocatorFailedSanityCheck;
  }
  const AllocatorState& valid_state = unsafe_state;

  SlotMetadata slot;
  AllocatorState::ErrorType error_type;
  auto ret =
      valid_state.GetMetadataForAddress(exception_addr, &slot, &error_type);
  if (ret == AllocatorState::GetMetadataReturnType::kErrorBadSlot) {
    DLOG(ERROR) << "Allocator computed a bad slot index!";
    return GwpAsanCrashAnalysisResult::kErrorBadSlot;
  }
  if (ret == AllocatorState::GetMetadataReturnType::kUnrelatedCrash)
    return GwpAsanCrashAnalysisResult::kUnrelatedCrash;

  proto->set_error_type(static_cast<Crash_ErrorType>(error_type));
  proto->set_allocation_address(slot.alloc_ptr);
  proto->set_allocation_size(slot.alloc_size);
  if (slot.alloc.tid != base::kInvalidThreadId || slot.alloc.trace_len)
    ReadAllocationInfo(memory, slot.alloc, proto->mutable_allocation());
  if (slot.dealloc.tid != base::kInvalidThreadId || slot.dealloc.trace_len)
    ReadAllocationInfo(memory, slot.dealloc, proto->mutable_deallocation());

  return GwpAsanCrashAnalysisResult::kGwpAsanCrash;
}

bool CrashAnalyzer::ReadAllocationInfo(
    const crashpad::ProcessMemory& memory,
    const SlotMetadata::AllocationInfo& slot_info,
    gwp_asan::Crash_AllocationInfo* proto_info) {
  if (slot_info.tid != base::kInvalidThreadId)
    proto_info->set_thread_id(slot_info.tid);

  if (!slot_info.trace_len)
    return true;

  size_t trace_len = std::min(slot_info.trace_len, kMaxStackTraceLen);
  // On 32-bit platforms we can't read directly to
  // proto_info->mutable_stack_trace()->mutable_data(), so we read to an
  // intermediate uintptr_t array.
  uintptr_t trace[kMaxStackTraceLen];
  if (!ReadStackTrace(memory,
                      static_cast<crashpad::VMAddress>(slot_info.trace_addr),
                      trace, trace_len))
    return false;

  proto_info->mutable_stack_trace()->Resize(trace_len, 0);
  uint64_t* output = proto_info->mutable_stack_trace()->mutable_data();
  for (size_t i = 0; i < trace_len; i++)
    output[i] = trace[i];

  return true;
}

bool CrashAnalyzer::ReadStackTrace(const crashpad::ProcessMemory& memory,
                                   crashpad::VMAddress crashing_trace_addr,
                                   uintptr_t* trace_output,
                                   size_t trace_len) {
  if (!memory.Read(crashing_trace_addr, sizeof(uintptr_t) * trace_len,
                   trace_output)) {
    DLOG(ERROR) << "Memory read should always succeed.";
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace gwp_asan
