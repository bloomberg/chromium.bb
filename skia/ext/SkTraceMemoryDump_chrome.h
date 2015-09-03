// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SK_TRACE_MEMORY_DUMP_CHROME_H_
#define SKIA_EXT_SK_TRACE_MEMORY_DUMP_CHROME_H_

#include <string>

#include "base/macros.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"

namespace base {
namespace trace_event {
class MemoryAllocatorDump;
class ProcessMemoryDump;
}
}

namespace skia {

class SkTraceMemoryDump_Chrome : public SkTraceMemoryDump {
 public:
  // This should never outlive the OnMemoryDump call since the
  // ProcessMemoryDump is valid only in that timeframe. Optional
  // |dump_name_prefix| argument specifies the prefix appended to the dump
  // name skia provides. By defualt it is taken as empty string.
  SkTraceMemoryDump_Chrome(
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  SkTraceMemoryDump_Chrome(
      const char* dump_name_prefix,
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  ~SkTraceMemoryDump_Chrome() override;

  // SkTraceMemoryDump implementation:
  void dumpNumericValue(const char* dumpName,
                        const char* valueName,
                        const char* units,
                        uint64_t value) override;
  void setMemoryBacking(const char* dumpName,
                        const char* backingType,
                        const char* backingObjectId) override;
  void setDiscardableMemoryBacking(
      const char* dumpName,
      const SkDiscardableMemory& discardableMemoryObject) override;

 protected:
  base::trace_event::ProcessMemoryDump* process_memory_dump() {
    return process_memory_dump_;
  }

 private:
  base::trace_event::ProcessMemoryDump* process_memory_dump_;

  std::string dump_name_prefix_;

  // Helper to create allocator dumps.
  base::trace_event::MemoryAllocatorDump* GetOrCreateAllocatorDump(
      const char* dumpName);

  DISALLOW_COPY_AND_ASSIGN(SkTraceMemoryDump_Chrome);
};

}  // namespace skia

#endif  // SKIA_EXT_SK_TRACE_MEMORY_DUMP_CHROME_H_
