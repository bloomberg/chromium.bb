// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/v8_unwinder.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace {

class V8Module : public base::ModuleCache::Module {
 public:
  explicit V8Module(const v8::MemoryRange& memory_range)
      : memory_range_(memory_range) {}

  V8Module(const V8Module&) = delete;
  V8Module& operator=(const V8Module&) = delete;

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override {
    return reinterpret_cast<uintptr_t>(memory_range_.start);
  }

  std::string GetId() const override {
    // We don't want to distinguish V8 code by memory region so we use the same
    // synthetic build id for all V8Modules.
    return V8Unwinder::kV8CodeRangeBuildId;
  }

  base::FilePath GetDebugBasename() const override {
    return base::FilePath().AppendASCII("V8 Code Range");
  }

  size_t GetSize() const override { return memory_range_.length_in_bytes; }

  bool IsNative() const override { return false; }

 private:
  const v8::MemoryRange memory_range_;
};

// Heterogeneous comparator for MemoryRanges and Modules. Compares on both
// base address and size because the module sizes can be updated while the
// base address remains the same.
struct MemoryRangeModuleCompare {
  bool operator()(const v8::MemoryRange& range,
                  const base::ModuleCache::Module* module) const {
    return std::make_pair(reinterpret_cast<uintptr_t>(range.start),
                          range.length_in_bytes) <
           std::make_pair(module->GetBaseAddress(), module->GetSize());
  }

  bool operator()(const base::ModuleCache::Module* module,
                  const v8::MemoryRange& range) const {
    return std::make_pair(module->GetBaseAddress(), module->GetSize()) <
           std::make_pair(reinterpret_cast<uintptr_t>(range.start),
                          range.length_in_bytes);
  }

  bool operator()(const v8::MemoryRange& a, const v8::MemoryRange& b) const {
    return std::make_pair(a.start, a.length_in_bytes) <
           std::make_pair(b.start, b.length_in_bytes);
  }
};

}  // namespace

V8Unwinder::V8Unwinder(v8::Isolate* isolate)
    : isolate_(isolate), js_entry_stubs_(isolate->GetJSEntryStubs()) {}

V8Unwinder::~V8Unwinder() = default;

// IMPORTANT NOTE: to avoid deadlock this function must not invoke any
// non-reentrant code that is also invoked by the target thread. In particular,
// no heap allocation or deallocation is permitted, including indirectly via use
// of DCHECK/CHECK or other logging statements.
void V8Unwinder::OnStackCapture() {
  required_code_ranges_capacity_ =
      CopyCodePages(code_ranges_.capacity(), code_ranges_.buffer());
  code_ranges_.SetSize(
      std::min(required_code_ranges_capacity_, code_ranges_.capacity()));
}

void V8Unwinder::UpdateModules(base::ModuleCache* module_cache) {
  MemoryRangeModuleCompare less_than;

  std::vector<std::unique_ptr<const base::ModuleCache::Module>> new_modules;
  std::vector<const base::ModuleCache::Module*> defunct_modules;

  // Identify defunct modules and create new modules seen since the last
  // sample. Code ranges provided by V8 are in sorted order.
  v8::MemoryRange* const code_ranges_start = code_ranges_.buffer();
  v8::MemoryRange* const code_ranges_end =
      code_ranges_start + code_ranges_.size();
  DCHECK(std::is_sorted(code_ranges_start, code_ranges_end, less_than));
  v8::MemoryRange* range_it = code_ranges_start;
  auto modules_it = modules_.begin();
  while (range_it != code_ranges_end && modules_it != modules_.end()) {
    if (less_than(*range_it, *modules_it)) {
      new_modules.push_back(std::make_unique<V8Module>(*range_it));
      modules_.insert(modules_it, new_modules.back().get());
      ++range_it;
    } else if (less_than(*modules_it, *range_it)) {
      defunct_modules.push_back(*modules_it);
      modules_it = modules_.erase(modules_it);
    } else {
      // The range already has a module, so there's nothing to do.
      ++range_it;
      ++modules_it;
    }
  }

  while (range_it != code_ranges_end) {
    new_modules.push_back(std::make_unique<V8Module>(*range_it));
    modules_.insert(modules_it, new_modules.back().get());
    ++range_it;
  }

  while (modules_it != modules_.end()) {
    defunct_modules.push_back(*modules_it);
    modules_it = modules_.erase(modules_it);
  }

  module_cache->UpdateNonNativeModules(defunct_modules, std::move(new_modules));
  code_ranges_.ExpandCapacityIfNecessary(required_code_ranges_capacity_);
}

bool V8Unwinder::CanUnwindFrom(const base::Frame& current_frame) const {
  const base::ModuleCache::Module* module = current_frame.module;
  if (!module)
    return false;
  const auto loc = modules_.find(module);
  DCHECK(loc == modules_.end() || *loc == module);
  return loc != modules_.end();
}

base::UnwindResult V8Unwinder::TryUnwind(
    base::RegisterContext* thread_context,
    uintptr_t stack_top,
    base::ModuleCache* module_cache,
    std::vector<base::Frame>* stack) const {
  v8::RegisterState register_state;
  register_state.pc = reinterpret_cast<void*>(
      base::RegisterContextInstructionPointer(thread_context));
  register_state.sp = reinterpret_cast<void*>(
      base::RegisterContextStackPointer(thread_context));
  register_state.fp = reinterpret_cast<void*>(
      base::RegisterContextFramePointer(thread_context));

  if (!v8::Unwinder::TryUnwindV8Frames(
          js_entry_stubs_, code_ranges_.size(), code_ranges_.buffer(),
          &register_state, reinterpret_cast<const void*>(stack_top))) {
    return base::UnwindResult::ABORTED;
  }

  const uintptr_t prev_stack_pointer =
      base::RegisterContextStackPointer(thread_context);
  DCHECK_GT(reinterpret_cast<uintptr_t>(register_state.sp), prev_stack_pointer);
  DCHECK_LT(reinterpret_cast<uintptr_t>(register_state.sp), stack_top);

  base::RegisterContextInstructionPointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.pc);
  base::RegisterContextStackPointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.sp);
  base::RegisterContextFramePointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.fp);

  stack->emplace_back(
      base::RegisterContextInstructionPointer(thread_context),
      module_cache->GetModuleForAddress(
          base::RegisterContextInstructionPointer(thread_context)));

  return base::UnwindResult::UNRECOGNIZED_FRAME;
}

size_t V8Unwinder::CopyCodePages(size_t capacity, v8::MemoryRange* code_pages) {
  return isolate_->CopyCodePages(capacity, code_pages);
}

// Synthetic build id to use for V8 modules.
const char V8Unwinder::kV8CodeRangeBuildId[] =
    "5555555507284E1E874EFA4EB754964B999";

V8Unwinder::MemoryRanges::MemoryRanges()
    : capacity_(v8::Isolate::kMinCodePagesBufferSize),
      size_(0),
      ranges_(std::make_unique<v8::MemoryRange[]>(capacity_)) {}

V8Unwinder::MemoryRanges::MemoryRanges::~MemoryRanges() = default;

void V8Unwinder::MemoryRanges::SetSize(size_t size) {
  // DCHECKing size_ <= capacity_ is deferred to size() because the DCHECK may
  // heap allocate.
  size_ = size;
}

void V8Unwinder::MemoryRanges::ExpandCapacityIfNecessary(
    size_t required_capacity) {
  if (required_capacity > capacity_) {
    while (required_capacity > capacity_)
      capacity_ *= 2;
    auto new_ranges = std::make_unique<v8::MemoryRange[]>(capacity_);
    std::copy(buffer(), buffer() + size_, new_ranges.get());
    ranges_ = std::move(new_ranges);
  }
}

bool V8Unwinder::ModuleCompare::operator()(
    const base::ModuleCache::Module* a,
    const base::ModuleCache::Module* b) const {
  return std::make_pair(a->GetBaseAddress(), a->GetSize()) <
         std::make_pair(b->GetBaseAddress(), b->GetSize());
}
