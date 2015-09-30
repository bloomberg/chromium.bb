// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/SkTraceMemoryDump_chrome.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "skia/ext/SkDiscardableMemory_chrome.h"

namespace skia {

namespace {
const char kMallocBackingType[] = "malloc";
}

SkTraceMemoryDump_Chrome::SkTraceMemoryDump_Chrome(
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
    base::trace_event::ProcessMemoryDump* process_memory_dump)
    : SkTraceMemoryDump_Chrome("", level_of_detail, process_memory_dump) {}

SkTraceMemoryDump_Chrome::SkTraceMemoryDump_Chrome(
    const std::string& dump_name_prefix,
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
    base::trace_event::ProcessMemoryDump* process_memory_dump)
    : dump_name_prefix_(dump_name_prefix),
      process_memory_dump_(process_memory_dump),
      request_level_(
          level_of_detail == base::trace_event::MemoryDumpLevelOfDetail::LIGHT
              ? SkTraceMemoryDump::kLight_LevelOfDetail
              : SkTraceMemoryDump::kObjectsBreakdowns_LevelOfDetail) {}

SkTraceMemoryDump_Chrome::~SkTraceMemoryDump_Chrome() {}

void SkTraceMemoryDump_Chrome::dumpNumericValue(const char* dumpName,
                                                const char* valueName,
                                                const char* units,
                                                uint64_t value) {
  auto dump = GetOrCreateAllocatorDump(dumpName);
  dump->AddScalar(valueName, units, value);
}

void SkTraceMemoryDump_Chrome::setMemoryBacking(const char* dumpName,
                                                const char* backingType,
                                                const char* backingObjectId) {
  if (strcmp(backingType, kMallocBackingType) == 0) {
    auto dump = GetOrCreateAllocatorDump(dumpName);
    const char* system_allocator_name =
        base::trace_event::MemoryDumpManager::GetInstance()
            ->system_allocator_pool_name();
    if (system_allocator_name) {
      process_memory_dump_->AddSuballocation(dump->guid(),
                                             system_allocator_name);
    }
  } else {
    NOTREACHED();
  }
}

void SkTraceMemoryDump_Chrome::setDiscardableMemoryBacking(
    const char* dumpName,
    const SkDiscardableMemory& discardableMemoryObject) {
  std::string name = dump_name_prefix_ + dumpName;
  DCHECK(!process_memory_dump_->GetAllocatorDump(name));
  const SkDiscardableMemoryChrome& discardable_memory_obj =
      static_cast<const SkDiscardableMemoryChrome&>(discardableMemoryObject);
  auto dump = discardable_memory_obj.CreateMemoryAllocatorDump(
      name.c_str(), process_memory_dump_);
  DCHECK(dump);
}

SkTraceMemoryDump::LevelOfDetail SkTraceMemoryDump_Chrome::getRequestedDetails()
    const {
  return request_level_;
}

base::trace_event::MemoryAllocatorDump*
SkTraceMemoryDump_Chrome::GetOrCreateAllocatorDump(const char* dumpName) {
  std::string name = dump_name_prefix_ + dumpName;
  auto dump = process_memory_dump_->GetAllocatorDump(name);
  if (!dump)
    dump = process_memory_dump_->CreateAllocatorDump(name);
  return dump;
}

}  // namespace skia
