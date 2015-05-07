// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebProcessMemoryDump_h
#define WebProcessMemoryDump_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

class WebMemoryAllocatorDump;

// A container which holds all the dumps for the various allocators for a given
// process. Embedders of WebMemoryDumpProvider are expected to populate a
// WebProcessMemoryDump instance with the stats of their allocators.
class BLINK_PLATFORM_EXPORT WebProcessMemoryDump {
public:
    virtual ~WebProcessMemoryDump();

    // Creates a new MemoryAllocatorDump with the given name and returns the
    // empty object back to the caller. |absoluteName| uniquely identifies the
    // dump within the scope of a ProcessMemoryDump. It is possible to express
    // nesting by means of a slash-separated path naming (e.g.,
    // "allocator_name/arena_1/subheap_X").
    virtual WebMemoryAllocatorDump* createMemoryAllocatorDump(const WebString& absoluteName)
    {
        return nullptr;
    }
};

} // namespace blink

#endif // WebProcessMemoryDump_h
