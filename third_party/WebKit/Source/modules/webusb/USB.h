// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBClient.h"

namespace blink {

class LocalFrame;
class ScriptState;
class USBDeviceRequestOptions;
class WebUSBDevice;

class USB final
    : public EventTargetWithInlineData
    , public ContextLifecycleObserver
    , public WebUSBClient::Observer {
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(USB);
    USING_GARBAGE_COLLECTED_MIXIN(USB);
    USING_PRE_FINALIZER(USB, dispose);
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
    ExecutionContext* getExecutionContext() const override;
    const AtomicString& interfaceName() const override;

    // ContextLifecycleObserver overrides.
    void contextDestroyed() override;

    // WebUSBClient::Observer overrides.
    void onDeviceConnected(std::unique_ptr<WebUSBDevice>) override;
    void onDeviceDisconnected(std::unique_ptr<WebUSBDevice>) override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit USB(LocalFrame& frame);
    void dispose();

    WebUSBClient* m_client;
};

} // namespace blink

#endif // USB_h
