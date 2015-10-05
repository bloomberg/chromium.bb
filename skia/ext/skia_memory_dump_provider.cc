// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia_memory_dump_provider.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "skia/ext/skia_trace_memory_dump_impl.h"
#include "third_party/skia/include/core/SkGraphics.h"

namespace skia {

// static
SkiaMemoryDumpProvider* SkiaMemoryDumpProvider::GetInstance() {
  return base::Singleton<
      SkiaMemoryDumpProvider,
      base::LeakySingletonTraits<SkiaMemoryDumpProvider>>::get();
}

SkiaMemoryDumpProvider::SkiaMemoryDumpProvider() {}

SkiaMemoryDumpProvider::~SkiaMemoryDumpProvider() {}

bool SkiaMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  SkiaTraceMemoryDumpImpl skia_dumper(args.level_of_detail,
                                      process_memory_dump);
  SkGraphics::DumpMemoryStatistics(&skia_dumper);

  return true;
}

}  // namespace skia
