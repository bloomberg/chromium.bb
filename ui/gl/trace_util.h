// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TRACE_UTIL_H_
#define UI_GL_TRACE_UTIL_H_

#include "base/trace_event/memory_allocator_dump.h"
#include "ui/gl/gl_export.h"

namespace gfx {

GL_EXPORT base::trace_event::MemoryAllocatorDumpGuid GetGLTextureGUIDForTracing(
    uint64_t tracing_process_id,
    uint32_t texture_id);

}  // namespace ui

#endif  // UI_GL_TRACE_UTIL_H_
