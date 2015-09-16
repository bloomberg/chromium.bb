// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "modules/webusb/USBController.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBClient.h"

namespace blink {

class LocalFrame;
class ScriptState;
class USBDeviceRequestOptions;
class WebUSBDevice;

class USB final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<USB>
    , public WebUSBClient::Observer {
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(USB);
public:
    static USB* create(LocalFrame& frame)
    {
        return new USB(frame);
    }

    // Eagerly finalize to enable to access m_controller in the destructor.
    EAGERLY_FINALIZE();
    ~USB() override;

    // USB.idl
    ScriptPromise getDevices(ScriptState*);
    ScriptPromise requestDevice(ScriptState*, const USBDeviceRequestOptions&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    // EventTarget overrides.
    ExecutionContext* executionContext() const override;
    const AtomicString& interfaceName() const override;

    // WebUSBClient::Observer overrides.
    void onDeviceConnected(WebPassOwnPtr<WebUSBDevice>) override;
    void onDeviceDisconnected(WebPassOwnPtr<WebUSBDevice>) override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit USB(LocalFrame& frame);

    RawPtrWillBeMember<USBController> m_controller;
};

} // namespace blink

#endif // USB_h
