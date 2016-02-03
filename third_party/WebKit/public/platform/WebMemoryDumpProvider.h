// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryDumpProvider_h
#define WebMemoryDumpProvider_h

#include "WebCommon.h"

namespace blink {

class WebProcessMemoryDump;

// Used to specify the type of memory dump the WebMemoryDumpProvider should
// generate on dump requests.
// TODO(hajimehoshi): Remove this and use base::trace_event::
// MemoryDumpLevelOfDetail instead.
enum class WebMemoryDumpLevelOfDetail {
    Light,
    Detailed
};

// Base interface to be part of the memory tracing infrastructure. Blink classes
// can implement this interface and register themselves (see
// Platform::registerMemoryDumpProvider()) to dump stats for their allocators.
class BLINK_PLATFORM_EXPORT WebMemoryDumpProvider {
public:
    // Function types for functions that can be called on alloc and free to do
    // heap profiling.
    typedef void AllocationHook(void* address, size_t, const char*);
    typedef void FreeHook(void* address);

    virtual ~WebMemoryDumpProvider();

    // Called by the MemoryDumpManager when generating memory dumps.
    // WebMemoryDumpLevelOfDetail specifies the size of dump the embedders
    // should generate on dump requests. Embedders are expected to populate
    // the WebProcessMemoryDump* argument depending on the level and return true
    // on success or false if anything went wrong and the dump should be
    // considered invalid.
    virtual bool onMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) = 0;

    // Because Blink cannot depend on base, heap profiling bookkeeping has to
    // be done in the glue layer for now. This method allows the glue layer to
    // detect whether the current dump provider supports heap profiling.
    // TODO(ruuda): Remove once wtf can depend on base and do bookkeeping in the
    // provider itself.
    virtual bool supportsHeapProfiling() { return false; }

    // Called by the memory dump manager to enable heap profiling (with true) or
    // called to disable heap profiling (with false).
    virtual void onHeapProfilingEnabled(bool enabled) {}
};

} // namespace blink

#endif // WebMemoryDumpProvider_h
