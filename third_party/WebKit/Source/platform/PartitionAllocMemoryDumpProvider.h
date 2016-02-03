// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PartitionAllocMemoryDumpProvider_h
#define PartitionAllocMemoryDumpProvider_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebMemoryDumpProvider.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace base {
namespace trace_event {

class AllocationRegister;

} // namespace trace_event
} // namespace base

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
    void onHeapProfilingEnabled(bool) override;

    // These methods are called only from PartitionAllocHooks' callbacks.
    void insert(void*, size_t, const char*);
    void remove(void*);

private:
    PartitionAllocMemoryDumpProvider();

    Mutex m_allocationRegisterMutex;
    OwnPtr<base::trace_event::AllocationRegister> m_allocationRegister;
    bool m_isHeapProfilingEnabled;
};

} // namespace blink

#endif // PartitionAllocMemoryDumpProvider_h
