// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontCacheMemoryDumpProvider_h
#define FontCacheMemoryDumpProvider_h

#include "platform/PlatformExport.h"
#include "public/platform/WebMemoryDumpProvider.h"
#include "wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT FontCacheMemoryDumpProvider final : public WebMemoryDumpProvider {
    USING_FAST_MALLOC(FontCacheMemoryDumpProvider);
public:
    static FontCacheMemoryDumpProvider* instance();
    ~FontCacheMemoryDumpProvider() override { }

    // WebMemoryDumpProvider implementation.
    bool onMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) override;

private:
    FontCacheMemoryDumpProvider() { }
};

} // namespace blink

#endif // FontCacheMemoryDumpProvider_h
