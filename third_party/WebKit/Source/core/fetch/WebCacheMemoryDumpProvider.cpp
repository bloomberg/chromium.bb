// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/WebCacheMemoryDumpProvider.h"

#include "core/fetch/MemoryCache.h"
#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"

namespace blink {

WebCacheMemoryDumpProvider* WebCacheMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(WebCacheMemoryDumpProvider, instance, ());
    return &instance;
}

bool WebCacheMemoryDumpProvider::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail, WebProcessMemoryDump* memoryDump)
{
    ASSERT(isMainThread());
    if (m_memoryCache)
        m_memoryCache->onMemoryDump(levelOfDetail, memoryDump);
    return true;
}

WebCacheMemoryDumpProvider::WebCacheMemoryDumpProvider()
{
}

WebCacheMemoryDumpProvider::~WebCacheMemoryDumpProvider()
{
}

} // namespace blink
