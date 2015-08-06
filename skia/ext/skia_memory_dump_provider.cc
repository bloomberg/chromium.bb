// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia_memory_dump_provider.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/src/core/SkResourceCache.h"

namespace skia {

// static
SkiaMemoryDumpProvider* SkiaMemoryDumpProvider::GetInstance() {
  return Singleton<SkiaMemoryDumpProvider,
                   LeakySingletonTraits<SkiaMemoryDumpProvider>>::get();
}

SkiaMemoryDumpProvider::SkiaMemoryDumpProvider() {}

SkiaMemoryDumpProvider::~SkiaMemoryDumpProvider() {}

bool SkiaMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  auto font_mad =
      process_memory_dump->CreateAllocatorDump("skia/sk_font_cache");
  font_mad->AddScalar("size", "bytes", SkGraphics::GetFontCacheUsed());
  font_mad->AddScalar("count", "objects", SkGraphics::GetFontCacheCountUsed());

  auto resource_mad =
      process_memory_dump->CreateAllocatorDump("skia/sk_resource_cache");
  resource_mad->AddScalar("size", "bytes",
                          SkResourceCache::GetTotalBytesUsed());
  // TODO(ssid): crbug.com/503168. Add sub-allocation edges from discardable or
  // malloc memory dumps to avoid double counting.

  return true;
}

}  // namespace skia
