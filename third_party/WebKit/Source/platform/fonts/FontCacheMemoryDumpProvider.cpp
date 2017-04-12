// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontCacheMemoryDumpProvider.h"

#include "platform/fonts/FontCache.h"
#include "platform/wtf/Assertions.h"

namespace blink {

FontCacheMemoryDumpProvider* FontCacheMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(FontCacheMemoryDumpProvider, instance, ());
  return &instance;
}

bool FontCacheMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs&,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  DCHECK(IsMainThread());
  FontCache::GetFontCache()->DumpFontPlatformDataCache(memory_dump);
  FontCache::GetFontCache()->DumpShapeResultCache(memory_dump);
  return true;
}

}  // namespace blink
