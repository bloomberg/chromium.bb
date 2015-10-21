// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCacheMemoryDumpProvider_h
#define WebCacheMemoryDumpProvider_h

#include "core/fetch/MemoryCache.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebMemoryDumpProvider.h"
#include "wtf/MainThread.h"

namespace blink {

// This class is wrapper around MemoryCache to take memory snapshots. It dumps
// the stats of cache only after the cache is created.
class CORE_EXPORT WebCacheMemoryDumpProvider final : public WebMemoryDumpProvider {
public:
    // This class is singleton since there is a global MemoryCache object.
    static WebCacheMemoryDumpProvider* instance();
    ~WebCacheMemoryDumpProvider() override;

    // WebMemoryDumpProvider implementation.
    bool onMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) override;

    void setMemoryCache(MemoryCache* memoryCache)
    {
        ASSERT(isMainThread());
        m_memoryCache = memoryCache;
    }

private:
    WebCacheMemoryDumpProvider();

    WeakPersistent<MemoryCache> m_memoryCache;
};

} // namespace blink

#endif // WebCacheMemoryDumpProvider_h
