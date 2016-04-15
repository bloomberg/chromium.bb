// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebProcessMemoryDumpImpl_h
#define WebProcessMemoryDumpImpl_h

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

#include <map>
#include <memory>
#include <vector>

namespace base {
class DiscardableMemory;
namespace trace_event {
class MemoryAllocatorDump;
class ProcessMemoryDump;
}  // namespace base
}  // namespace trace_event

namespace skia {
class SkiaTraceMemoryDumpImpl;
}  // namespace skia

namespace blink {

class WebMemoryAllocatorDumpImpl;

// Implements the blink::WebProcessMemoryDump interface by means of proxying the
// calls to createMemoryAllocatorDump() to the underlying
// base::trace_event::ProcessMemoryDump instance.
class PLATFORM_EXPORT WebProcessMemoryDumpImpl final
    : public NON_EXPORTED_BASE(blink::WebProcessMemoryDump) {
 public:
  // Creates a standalone WebProcessMemoryDumpImp, which owns the underlying
  // ProcessMemoryDump.
  WebProcessMemoryDumpImpl();

  // Wraps (without owning) an existing ProcessMemoryDump.
  explicit WebProcessMemoryDumpImpl(
      base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  ~WebProcessMemoryDumpImpl() override;

  // blink::WebProcessMemoryDump implementation.
  blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name) override;
  blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name,
      blink::WebMemoryAllocatorDumpGuid guid) override;
  blink::WebMemoryAllocatorDump* getMemoryAllocatorDump(
      const blink::WebString& absolute_name) const override;
  void clear() override;
  void takeAllDumpsFrom(blink::WebProcessMemoryDump* other) override;
  void addOwnershipEdge(blink::WebMemoryAllocatorDumpGuid source,
                        blink::WebMemoryAllocatorDumpGuid target,
                        int importance) override;
  void addOwnershipEdge(blink::WebMemoryAllocatorDumpGuid source,
                        blink::WebMemoryAllocatorDumpGuid target) override;
  void addSuballocation(blink::WebMemoryAllocatorDumpGuid source,
                        const blink::WebString& target_node_name) override;
  SkTraceMemoryDump* createDumpAdapterForSkia(
      const blink::WebString& dump_name_prefix) override;

  const base::trace_event::ProcessMemoryDump* process_memory_dump() const {
    return process_memory_dump_;
  }

  blink::WebMemoryAllocatorDump* createDiscardableMemoryAllocatorDump(
      const std::string& name,
      base::DiscardableMemory* discardable) override;

  void dumpHeapUsage(
      const base::hash_map<base::trace_event::AllocationContext, base::trace_event::AllocationMetrics>&
          metrics_by_context,
      base::trace_event::TraceEventMemoryOverhead& overhead,
      const char* allocator_name) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebProcessMemoryDumpImplTest, IntegrationTest);

  blink::WebMemoryAllocatorDump* createWebMemoryAllocatorDump(
      base::trace_event::MemoryAllocatorDump* memory_allocator_dump);

  // Only for the case of ProcessMemoryDump being owned (i.e. the default ctor).
  std::unique_ptr<base::trace_event::ProcessMemoryDump> owned_process_memory_dump_;

  // The underlying ProcessMemoryDump instance to which the
  // createMemoryAllocatorDump() calls will be proxied to.
  base::trace_event::ProcessMemoryDump* process_memory_dump_;  // Not owned.

  // TODO(ssid): Remove it once this information is added to ProcessMemoryDump.
  base::trace_event::MemoryDumpLevelOfDetail level_of_detail_;

  // Reverse index of MemoryAllocatorDump -> WebMemoryAllocatorDumpImpl wrapper.
  // By design WebMemoryDumpProvider(s) are not supposed to hold the pointer
  // to the WebProcessMemoryDump passed as argument of the onMemoryDump() call.
  // Those pointers are valid only within the scope of the call and can be
  // safely torn down once the WebProcessMemoryDumpImpl itself is destroyed.
  HashMap<base::trace_event::MemoryAllocatorDump*,
          OwnPtr<WebMemoryAllocatorDumpImpl>> memory_allocator_dumps_;

  // Stores SkTraceMemoryDump for the current ProcessMemoryDump.
  std::vector<std::unique_ptr<skia::SkiaTraceMemoryDumpImpl>> sk_trace_dump_list_;

  DISALLOW_COPY_AND_ASSIGN(WebProcessMemoryDumpImpl);
};

}  // namespace blink

#endif  // WebProcessMemoryDumpImpl_h
