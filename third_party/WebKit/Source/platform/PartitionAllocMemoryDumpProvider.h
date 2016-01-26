// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PartitionAllocMemoryDumpProvider_h
#define PartitionAllocMemoryDumpProvider_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebMemoryDumpProvider.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class BLINK_PLATFORM_EXPORT PartitionAllocMemoryDumpProvider final : public WebMemoryDumpProvider {
    USING_FAST_MALLOC(PartitionAllocMemoryDumpProvider);
    WTF_MAKE_NONCOPYABLE(PartitionAllocMemoryDumpProvider);
public:
    static PartitionAllocMemoryDumpProvider* instance();
    ~PartitionAllocMemoryDumpProvider() override;

    // WebMemoryDumpProvider implementation.
    bool onMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) override;
    bool supportsHeapProfiling() override { return true; }
    void onHeapProfilingEnabled(AllocationHook*, FreeHook*) override;

private:
    PartitionAllocMemoryDumpProvider();
};

} // namespace blink

#endif // PartitionAllocMemoryDumpProvider_h
