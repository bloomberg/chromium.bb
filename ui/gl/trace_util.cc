// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/trace_util.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

base::trace_event::MemoryAllocatorDumpGuid GetGLTextureGUIDForTracing(
    uint64_t tracing_process_id,
    uint32_t texture_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(base::StringPrintf(
      "gl-texture-x-process/%" PRIx64 "/%d", tracing_process_id, texture_id));
}

}  // namespace ui
