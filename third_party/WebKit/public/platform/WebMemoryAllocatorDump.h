// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryAllocatorDump_h
#define WebMemoryAllocatorDump_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

// A container which holds all the attributes of a particular dump for a given
// allocator.
class BLINK_PLATFORM_EXPORT WebMemoryAllocatorDump {
public:
    virtual ~WebMemoryAllocatorDump();

    // Adds a scalar attribute to the dump.
    // Arguments:
    //   name: name of the attribute. Typical names, emitted by most allocators
    //       dump providers are: "size" and "objects_count".
    //   units: the units for the attribute. Gives a hint to the trace-viewer UI
    //       about the semantics of the attribute.
    //       Currently supported values are "bytes" and "objects".
    //   value: the value of the attribute.
    virtual void AddScalar(const char* name, const char* units, uint64_t value) { }
    virtual void AddScalarF(const char* name, const char* units, double value) { }
    virtual void AddString(const char* name, const char* units, const WebString& value) { }
};

} // namespace blink

#endif // WebMemoryAllocatorDump_h
