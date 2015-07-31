// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBDevice_h
#define USBDevice_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class USBControlTransferParameters;

class USBDevice
    : public GarbageCollectedFinalized<USBDevice>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBDevice* create()
    {
        return new USBDevice;
    }

    USBDevice() { }

    virtual ~USBDevice() { }

    String guid() const
    {
        ASSERT_NOT_REACHED();
        return "";
    }

    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // USBDevice_h
