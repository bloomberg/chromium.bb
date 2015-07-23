// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

base::trace_event::MemoryAllocatorDumpGuid GetGpuMemoryBufferGUIDForTracing(
    uint64 tracing_process_id,
    GpuMemoryBufferId buffer_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gpumemorybuffer-x-process/%" PRIx64 "/%d",
                         tracing_process_id, buffer_id));
}

GpuMemoryBufferHandle::GpuMemoryBufferHandle()
    : type(EMPTY_BUFFER), id(0), handle(base::SharedMemory::NULLHandle()) {
}

}  // namespace gfx
