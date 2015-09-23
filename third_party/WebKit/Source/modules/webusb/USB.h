// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/frame/LocalFrameLifecycleObserver.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBClient.h"

namespace blink {

class LocalFrame;
class ScriptState;
class USBDeviceRequestOptions;
class WebUSBDevice;

class USB final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<USB>
    , public LocalFrameLifecycleObserver
    , public WebUSBClient::Observer {
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(USB);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(USB);
public:
    static USB* create(LocalFrame& frame)
    {
        return new USB(frame);
    }

    ~USB() override;

    // USB.idl
    ScriptPromise getDevices(ScriptState*);
    ScriptPromise requestDevice(ScriptState*, const USBDeviceRequestOptions&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    // EventTarget overrides.
    ExecutionContext* executionContext() const override;
    const AtomicString& interfaceName() const override;

    // LocalFrameLifecycleObserver overrides.
    void willDetachFrameHost() override;

    // WebUSBClient::Observer overrides.
    void onDeviceConnected(WebPassOwnPtr<WebUSBDevice>) override;
    void onDeviceDisconnected(WebPassOwnPtr<WebUSBDevice>) override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit USB(LocalFrame& frame);

    WebUSBClient* m_client;
};

} // namespace blink

#endif // USB_h
