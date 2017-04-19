// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer_tracing.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

base::trace_event::MemoryAllocatorDumpGuid GetSharedMemoryGUIDForTracing(
    uint64_t tracing_process_id,
    GpuMemoryBufferId buffer_id) {
  // TODO(hajimehoshi): This should be unified to shared memory GUIDs in
  // base/memory/shared_memory_tracker.cc
  return base::trace_event::MemoryAllocatorDumpGuid(base::StringPrintf(
      "shared_memory_gpu/%" PRIx64 "/%d", tracing_process_id, buffer_id.id));
}

}  // namespace gfx
