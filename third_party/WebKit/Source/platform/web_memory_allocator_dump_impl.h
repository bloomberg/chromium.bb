// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryAllocatorDumpImpl_h
#define WebMemoryAllocatorDumpImpl_h

#include <stdint.h>

#include "base/macros.h"
#include "public/platform/WebMemoryAllocatorDump.h"

namespace base {
namespace trace_event {
class MemoryAllocatorDump;
}  // namespace base
}  // namespace trace_event

namespace blink {

// Implements the blink::WebMemoryAllocatorDump interface by means of proxying
// the Add*() calls to the underlying base::trace_event::MemoryAllocatorDump
// instance.
class WebMemoryAllocatorDumpImpl : public blink::WebMemoryAllocatorDump {
 public:
  explicit WebMemoryAllocatorDumpImpl(
      base::trace_event::MemoryAllocatorDump* memory_allocator_dump);
  ~WebMemoryAllocatorDumpImpl() override;

  // blink::WebMemoryAllocatorDump implementation.
  void addScalar(const char* name, const char* units, uint64_t value) override;
  void addScalarF(const char* name, const char* units, double value) override;
  void addString(const char* name,
                 const char* units,
                 const blink::WebString& value) override;

  blink::WebMemoryAllocatorDumpGuid guid() const override;

 private:
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump_;  // Not owned.
  blink::WebMemoryAllocatorDumpGuid guid_;

  DISALLOW_COPY_AND_ASSIGN(WebMemoryAllocatorDumpImpl);
};

}  // namespace blink

#endif  // WebMemoryAllocatorDumpImpl_h
