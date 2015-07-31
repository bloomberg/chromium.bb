// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/webusb/USBController.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScriptState;
class USBDeviceEnumerationOptions;

class USB final
    : public GarbageCollectedFinalized<USB>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USB* create(LocalFrame& frame)
    {
        return new USB(frame);
    }

    explicit USB(LocalFrame& frame);

    ScriptPromise getDevices(ScriptState*, const USBDeviceEnumerationOptions&);

    DECLARE_VIRTUAL_TRACE();

private:
    RawPtrWillBeMember<USBController> m_controller;
};

} // namespace blink

#endif // USB_h
