// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/stack_unwinder_android.h"

#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>
#include "link.h"

#include <algorithm>

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/cfi_backtrace_android.h"
#include "libunwind.h"

using base::trace_event::CFIBacktraceAndroid;
using base::debug::MappedMemoryRegion;

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SamplingProfilerUnwindResult {
  kFutexSignalFailed = 0,
  kStackCopyFailed = 1,
  kUnwindInitFailed = 2,
  kHandlerUnwindFailed = 3,
  kFirstFrameUnmapped = 4,
  kMaxValue = kFirstFrameUnmapped,
};

void RecordUnwindResult(SamplingProfilerUnwindResult result) {
  UMA_HISTOGRAM_ENUMERATION("BackgroundTracing.SamplingProfilerUnwindResult",
                            result);
}

// Waitable event implementation with futex and without DCHECK(s), since signal
// handlers cannot allocate memory or use pthread api.
class AsyncSafeWaitableEvent {
 public:
  AsyncSafeWaitableEvent() { base::subtle::Release_Store(&futex_, 0); }
  ~AsyncSafeWaitableEvent() {}

  bool Wait() {
    // futex() can wake up spuriously if this memory address was previously used
    // for a pthread mutex. So, also check the condition.
    while (true) {
      int res = syscall(SYS_futex, &futex_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0,
                        nullptr, nullptr, 0);
      if (base::subtle::Acquire_Load(&futex_) != 0)
        return true;
      if (res != 0)
        return false;
    }
  }

  void Signal() {
    base::subtle::Release_Store(&futex_, 1);
    syscall(SYS_futex, &futex_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr,
            nullptr, 0);
  }

 private:
  int futex_;
};

// Scoped signal event that calls Signal on the AsyncSafeWaitableEvent at
// destructor.
class ScopedEventSignaller {
 public:
  ScopedEventSignaller(AsyncSafeWaitableEvent* event) : event_(event) {}
  ~ScopedEventSignaller() { event_->Signal(); }

 private:
  AsyncSafeWaitableEvent* event_;
};

// Unwinds from the given context using libunwind.
size_t UnwindWithLibunwind(unw_cursor_t* cursor,
                           const tracing::StackUnwinderAndroid* unwinder,
                           const uintptr_t stack_segment_base,
                           const void** out_trace,
                           const size_t max_depth,
                           uintptr_t* sp,
                           uintptr_t* ip) {
  uintptr_t previous_sp = 0;
  uintptr_t depth = 0;
  const uintptr_t initial_sp = *sp;
  do {
    unw_get_reg(cursor, UNW_REG_IP, ip);
    unw_get_reg(cursor, UNW_REG_SP, sp);
    DCHECK_GE(*sp, initial_sp);
    if (stack_segment_base > 0)
      DCHECK_LT(*sp, stack_segment_base);

    // If SP and IP did not change from previous frame, then unwinding failed.
    if (previous_sp == *sp &&
        *ip == reinterpret_cast<uintptr_t>(out_trace[depth - 1])) {
      break;
    }
    previous_sp = *sp;

    // If address is in chrome library, then use CFI unwinder since chrome might
    // not have EHABI unwind tables.
    if (CFIBacktraceAndroid::is_chrome_address(*ip))
      break;

    // Break if pc is not from any mapped region. Something went wrong while
    // unwinding.
    if (!unwinder->IsAddressMapped(*ip))
      break;

    // If it is chrome address, the cfi unwinder will include it.
    out_trace[depth++] = reinterpret_cast<void*>(*ip);
  } while (unw_step(cursor) && depth < max_depth - 1);
  return depth;
}

// Unwinds from given |cursor| readable by libunwind, and returns
// the number of frames added to the output. This function can unwind through
// android framework and then chrome functions. It cannot handle the cases when
// the chrome functions are called by android framework again, since we cannot
// create the right context for libunwind from chrome functions.
// TODO(ssid): This function should support unwinding from chrome to android
// libraries also.
size_t TraceStackWithContext(
    unw_cursor_t* cursor,
    CFIBacktraceAndroid* cfi_unwinder,
    const tracing::StackUnwinderAndroid* unwinder,
    const uintptr_t stack_segment_base,
    const std::vector<const tracing::StackUnwinderAndroid::JniMarker*>&
        jni_markers,
    const void** out_trace,
    const size_t max_depth) {
  size_t depth = 0;
  unw_word_t ip = 0, sp = 0;
  bool try_stack_search = true;
  unw_get_reg(cursor, UNW_REG_SP, &sp);
  unw_get_reg(cursor, UNW_REG_IP, &ip);
  const uintptr_t initial_sp = sp;

  if (tracing::StackUnwinderAndroid::kUseLibunwind) {
    depth += UnwindWithLibunwind(cursor, unwinder, stack_segment_base,
                                 out_trace, max_depth, &sp, &ip);
  }

  if (CFIBacktraceAndroid::is_chrome_address(ip)) {
    // Continue unwinding CFI unwinder if we found stack frame from chrome
    // library.
    uintptr_t lr = 0;
    unw_get_reg(cursor, UNW_ARM_LR, &lr);
    depth +=
        cfi_unwinder->Unwind(ip, sp, lr, out_trace + depth, max_depth - depth);
    try_stack_search = false;
  }
  if (depth >= max_depth)
    return depth;

  // Try unwinding the rest of frames from Jni markers on stack if present. This
  // is to skip trying to unwind art frames which do not have unwind
  // information.
  for (const auto* marker : jni_markers) {
    // Skip if we already walked past this marker.
    if (sp > marker->sp)
      continue;
    depth += cfi_unwinder->Unwind(marker->pc, marker->sp, /*lr=*/0,
                                  out_trace + depth, max_depth - depth);
    try_stack_search = false;
    if (depth >= max_depth)
      break;
  }

  // We tried all possible ways to unwind and failed. So, scan the stack to find
  // all chrome addresses and add them to stack trace. This would give us a lot
  // of false frames on the trace. The idea is to try to sanitize the trace on
  // server side or try unwinding after each search. The current version just
  // sends back all PCs until we figure out what is the best way to sanitize
  // the stack trace.
  if (try_stack_search) {
    // Search from beginning of stack, in case unwinding obtained bad offsets.
    uintptr_t* stack = reinterpret_cast<uintptr_t*>(initial_sp);
    // Add a nullptr to differentiate addresses found by unwinding and scanning.
    out_trace[depth++] = nullptr;
    while (depth < max_depth &&
           reinterpret_cast<uintptr_t>(stack) < stack_segment_base) {
      if (CFIBacktraceAndroid::is_chrome_address(
              reinterpret_cast<uintptr_t>(*stack))) {
        out_trace[depth++] = reinterpret_cast<void*>(*stack);
      }
      ++stack;
    }
  }

  if (depth == 0)
    RecordUnwindResult(SamplingProfilerUnwindResult::kFirstFrameUnmapped);
  return depth;
}

uintptr_t RewritePointerIfInOriginalStack(uintptr_t addr,
                                          uintptr_t sp,
                                          uintptr_t stack_size,
                                          uintptr_t new_stack_top) {
  if (addr >= sp && addr < sp + stack_size)
    return addr - sp + new_stack_top;
  return addr;
}

// Creates unwind cursor for the copied stack, which points to the function
// frame in which the sampled thread was stopped. We get information about this
// frame from signal context. Replaces registers in the context and cursor to
// point to the new stack's top function frame.
bool GetUnwindCursorForStack(uintptr_t sp,
                             size_t stack_size,
                             uintptr_t new_stack_top,
                             const ucontext_t& signal_context,
                             unw_context_t* context,
                             unw_cursor_t* cursor) {
  // Initialize an unwind cursor on copied stack.
  if (unw_init_local(cursor, context) != 0)
    return false;

  uintptr_t return_sp = signal_context.uc_mcontext.arm_sp - sp + new_stack_top;

  // Reset the unwind cursor to previous function and continue with libunwind.
  unw_set_reg(cursor, UNW_REG_SP, return_sp);  // 13
  unw_set_reg(cursor, UNW_ARM_R0,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r0,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R1,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r1,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R3,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r2,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R3,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r3,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R4,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r4,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R5,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r5,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R6,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r6,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R7,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r7,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R8,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r8,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R9,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r9,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(
      cursor, UNW_ARM_R10,
      RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_r10, sp,
                                      stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R11,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_fp,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_R12,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_ip,
                                              sp, stack_size, new_stack_top));
  unw_set_reg(cursor, UNW_ARM_LR,
              RewritePointerIfInOriginalStack(signal_context.uc_mcontext.arm_lr,
                                              sp, stack_size, new_stack_top));

  // Setting the IP register might cause adjustments in SP register. So, this
  // must be set after setting SP to the right value.
  unw_set_reg(cursor, UNW_REG_IP, signal_context.uc_mcontext.arm_pc);  // 15

  return true;
}

// Struct to store the arguments to the signal handler.
struct HandlerParams {
  const tracing::StackUnwinderAndroid* unwinder;
  // The event is signalled when signal handler is done executing.
  AsyncSafeWaitableEvent* event;

  // Return values:
  // Successfully copied the stack segment.
  bool* success;
  // The register context of the thread used by libunwind.
  unw_context_t* context;
  // The value of Stack pointer of the thread.
  uintptr_t* sp;
  // The context of the return function from signal context.
  ucontext_t* ucontext;
  // Buffer to copy the stack segment.
  base::NativeStackSampler::StackBuffer* stack_buffer;
  size_t* stack_size;
};

// Argument passed to the ThreadSignalHandler() from the sampling thread to the
// sampled (stopped) thread. This value is set just before sending kill signal
// to the thread and reset when handler is done.
base::subtle::AtomicWord g_handler_params;

// The signal handler is called on the stopped thread as an additional stack
// frame. This relies on no alternate sigaltstack() being set. This function
// skips the handler frame on stack and unwinds the rest of the stack frames.
// This function should use async-safe functions only. The only call that could
// allocate memory on heap would be the cache in cfi unwinder. We need to ensure
// that AllocateCacheForCurrentThread() is called on the stopped thread before
// trying to get stack trace from the thread. See
// https://www.gnu.org/software/libc/manual/html_node/Nonreentrancy.html#Nonreentrancy.
static void ThreadSignalHandler(int n, siginfo_t* siginfo, void* sigcontext) {
  HandlerParams* params = reinterpret_cast<HandlerParams*>(
      base::subtle::Acquire_Load(&g_handler_params));
  ScopedEventSignaller e(params->event);
  *params->success = false;

  uintptr_t sp = 0;
  if (unw_getcontext(params->context) != 0)
    return;

  asm volatile("mov %0, sp" : "=r"(sp));
  *params->sp = sp;

  memcpy(params->ucontext, sigcontext, sizeof(ucontext_t));

  uintptr_t stack_base_addr = params->unwinder->GetEndAddressOfRegion(sp);
  *params->stack_size = stack_base_addr - sp;
  if (stack_base_addr == 0 ||
      *params->stack_size > params->stack_buffer->size())
    return;
  memcpy(params->stack_buffer->buffer(), reinterpret_cast<void*>(sp),
         *params->stack_size);
  *params->success = true;
}

// ARM EXIDX table contains addresses in sorted order with unwind data, each of
// 32 bits.
struct FakeExidx {
  uintptr_t pc;
  uintptr_t index_data;
};

}  // namespace

extern "C" {

_Unwind_Ptr __real_dl_unwind_find_exidx(_Unwind_Ptr, int*);

// Override the default |dl_unwind_find_exidx| function used by libunwind to
// give a fake unwind table just for the handler function. Otherwise call the
// original function. Libunwind marks the cursor invalid if it finds even one
// frame without unwind info. Mocking the info keeps the unwind cursor valid
// after unwind_init_local() within ThreadSignalHandler().
__attribute__((visibility("default"), noinline)) _Unwind_Ptr
__wrap_dl_unwind_find_exidx(_Unwind_Ptr pc, int* length) {
  if (!CFIBacktraceAndroid::is_chrome_address(pc)) {
    return __real_dl_unwind_find_exidx(pc, length);
  }
  // Fake exidx table that is passed to libunwind to work with chrome functions.
  // 0x80000000 has high bit set to 1. This means the unwind data is inline and
  // not in exception table (section 5 EHABI). 0 on the second high byte causes
  // a 0 proceedure to be lsda. But this is never executed since the pc and sp
  // will be overridden, before calling unw_step.
  static const FakeExidx chrome_exidx_data[] = {
      {CFIBacktraceAndroid::executable_start_addr(), 0x80000000},
      {CFIBacktraceAndroid::executable_end_addr(), 0x80000000}};
  *length = sizeof(chrome_exidx_data);
  return reinterpret_cast<_Unwind_Ptr>(chrome_exidx_data);
}

}  // extern "C"

namespace tracing {

// static
// Disable usage of libunwind till crbug/888434 if fixed.
const bool StackUnwinderAndroid::kUseLibunwind = false;

StackUnwinderAndroid::StackUnwinderAndroid() {}
StackUnwinderAndroid::~StackUnwinderAndroid() {}

void StackUnwinderAndroid::Initialize() {
  is_initialized_ = true;

  // Ensure Chrome unwinder is initialized.
  CFIBacktraceAndroid::GetInitializedInstance();

  // Parses /proc/self/maps.
  std::string contents;
  if (!base::debug::ReadProcMaps(&contents))
    NOTREACHED();
  std::vector<base::debug::MappedMemoryRegion> regions;
  if (!base::debug::ParseProcMaps(contents, &regions))
    NOTREACHED();

  // Remove any regions mapped to art java code, so that unwinder doesn't try to
  // walk past java frames. Walking java frames causes crashes, crbug/888434.
  base::EraseIf(regions, [](const base::debug::MappedMemoryRegion& region) {
    return region.path.empty() ||
           base::EndsWith(region.path, ".art", base::CompareCase::SENSITIVE) ||
           base::EndsWith(region.path, ".oat", base::CompareCase::SENSITIVE) ||
           base::EndsWith(region.path, ".jar", base::CompareCase::SENSITIVE) ||
           base::EndsWith(region.path, ".vdex", base::CompareCase::SENSITIVE);
  });
  std::sort(regions.begin(), regions.end(),
            [](const MappedMemoryRegion& a, const MappedMemoryRegion& b) {
              return a.start < b.start;
            });
  regions_.swap(regions);
}

size_t StackUnwinderAndroid::TraceStack(const void** out_trace,
                                        size_t max_depth) const {
  DCHECK(is_initialized_);
  unw_cursor_t cursor;
  unw_context_t context;

  if (unw_getcontext(&context) != 0)
    return 0;
  if (unw_init_local(&cursor, &context) != 0)
    return 0;
  return TraceStackWithContext(
      &cursor, CFIBacktraceAndroid::GetInitializedInstance(), this,
      /* stack_segment_base=*/0, std::vector<const JniMarker*>(), out_trace,
      max_depth);
}

size_t StackUnwinderAndroid::TraceStack(
    base::PlatformThreadId tid,
    base::NativeStackSampler::StackBuffer* stack_buffer,
    const void** out_trace,
    size_t max_depth) const {
  // Stops the thread with given tid with a signal handler. The signal handler
  // copies the stack of the thread and returns. This function tries to unwind
  // stack frames from the copied stack.
  DCHECK(is_initialized_);
  size_t stack_size;
  unw_context_t context;
  uintptr_t sp = 0;
  ucontext_t signal_context = {};
  if (!SuspendThreadAndRecordStack(tid, stack_buffer, &sp, &stack_size,
                                   &context, &signal_context)) {
    RecordUnwindResult(SamplingProfilerUnwindResult::kStackCopyFailed);
    return 0;
  }

  const uintptr_t new_stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer->buffer());
  uintptr_t ip = signal_context.uc_mcontext.arm_pc;
  uintptr_t return_sp = signal_context.uc_mcontext.arm_sp - sp + new_stack_top;

  auto* cfi_unwinder = CFIBacktraceAndroid::GetInitializedInstance();
  // Do not use libunwind if we stopped at chrome frame.
  if (CFIBacktraceAndroid::is_chrome_address(ip)) {
    return cfi_unwinder->Unwind(
        ip, return_sp, signal_context.uc_mcontext.arm_lr, out_trace, max_depth);
  }

  std::vector<const JniMarker*> jni_markers =
      RewritePointersAndGetMarkers(stack_buffer, sp, stack_size);

  unw_cursor_t cursor;
  if (!GetUnwindCursorForStack(sp, stack_size, new_stack_top, signal_context,
                               &context, &cursor)) {
    RecordUnwindResult(SamplingProfilerUnwindResult::kUnwindInitFailed);
    return 0;
  }

  return TraceStackWithContext(
      &cursor, cfi_unwinder, this,
      reinterpret_cast<uintptr_t>(stack_buffer->buffer()) + stack_size,
      jni_markers, out_trace, max_depth);
}

uintptr_t StackUnwinderAndroid::GetEndAddressOfRegion(uintptr_t addr) const {
  auto it =
      std::upper_bound(regions_.begin(), regions_.end(), addr,
                       [](uintptr_t addr, const MappedMemoryRegion& region) {
                         return addr < region.start;
                       });
  if (it == regions_.begin())
    return 0;
  --it;
  if (it->end > addr)
    return it->end;
  return 0;
}

bool StackUnwinderAndroid::IsAddressMapped(uintptr_t pc) const {
  // TODO(ssid): We only need to check regions which are file mapped.
  return GetEndAddressOfRegion(pc) != 0;
}

bool StackUnwinderAndroid::SuspendThreadAndRecordStack(
    base::PlatformThreadId tid,
    base::NativeStackSampler::StackBuffer* stack_buffer,
    uintptr_t* sp,
    size_t* stack_size,
    unw_context_t* context,
    ucontext_t* signal_context) const {
  AsyncSafeWaitableEvent wait_event;
  bool copied = false;
  HandlerParams params = {this, &wait_event,    &copied,      context,
                          sp,   signal_context, stack_buffer, stack_size};
  base::subtle::Release_Store(&g_handler_params,
                              reinterpret_cast<uintptr_t>(&params));

  // Change the signal handler for the thread to unwind function, which should
  // execute on the stack so that we will be able to unwind.
  struct sigaction act;
  struct sigaction oact;
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = ThreadSignalHandler;
  act.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&act.sa_mask);
  // SIGURG is chosen here because we observe no crashes with this signal and
  // neither Chrome or the AOSP sets up a special handler for this signal.
  if (!sigaction(SIGURG, &act, &oact)) {
    kill(tid, SIGURG);
    bool finished_waiting = wait_event.Wait();

    bool changed = sigaction(SIGURG, &oact, &act) == 0;
    DCHECK(changed);
    if (!finished_waiting) {
      RecordUnwindResult(SamplingProfilerUnwindResult::kFutexSignalFailed);
      NOTREACHED();
      return false;
    }
  }
  base::subtle::Release_Store(&g_handler_params, 0);
  return copied;
}

std::vector<const StackUnwinderAndroid::JniMarker*>
StackUnwinderAndroid::RewritePointersAndGetMarkers(
    base::NativeStackSampler::StackBuffer* stack_buffer,
    uintptr_t sp,
    size_t stack_size) const {
  std::vector<const JniMarker*> jni_markers;
  uintptr_t* new_stack = reinterpret_cast<uintptr_t*>(stack_buffer->buffer());
  constexpr uint32_t marker_l =
                         jni_generator::kJniStackMarkerValue & 0xFFFFFFFF;
  constexpr uint32_t marker_r = jni_generator::kJniStackMarkerValue >> 32;
  const uintptr_t new_stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer->buffer());
  const size_t ptrs_to_rewrite = stack_size / sizeof(uintptr_t);
  for (size_t i = 0; i < ptrs_to_rewrite; ++i) {
    // Scanning needs to be fixed for 64 bit version.
    DCHECK_EQ(4u, sizeof(uintptr_t));
    if (i < ptrs_to_rewrite - 1 && new_stack[i] == marker_l &&
        new_stack[i + 1] == marker_r) {
      // Note: JniJavaCallContext::sp will be replaced with offset below.
      const JniMarker* marker =
          reinterpret_cast<const JniMarker*>(new_stack + i);
      DCHECK_EQ(jni_generator::kJniStackMarkerValue, marker->marker);
      if (marker->sp >= sp && marker->sp < sp + stack_size &&
          CFIBacktraceAndroid::is_chrome_address(marker->pc)) {
        jni_markers.push_back(marker);
      } else {
        NOTREACHED();
      }
    }

    // Unwind can use address on the stack. So, replace them as well. See EHABI
    // #7.5.4 table 3.
    if (new_stack[i] >= sp && new_stack[i] < sp + stack_size)
      new_stack[i] = new_stack[i] - sp + new_stack_top;
  }
  return jni_markers;
}

}  // namespace tracing
