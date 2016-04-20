// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "device/usb/public/interfaces/chooser_service.mojom-wtf.h"
#include "device/usb/public/interfaces/device_manager.mojom-wtf.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScopedScriptPromiseResolver;
class ScriptState;
class USBDeviceRequestOptions;

class USB final
    : public EventTargetWithInlineData
    , public ContextLifecycleObserver {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(USB);
public:
    static USB* create(LocalFrame& frame)
    {
        return new USB(frame);
    }

    virtual ~USB();

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

    device::usb::wtf::DeviceManager* deviceManager() const { return m_deviceManager.get(); }

    void onGetDevices(ScriptPromiseResolver*, mojo::WTFArray<device::usb::wtf::DeviceInfoPtr>);
    void onGetPermission(ScriptPromiseResolver*, device::usb::wtf::DeviceInfoPtr);
    void onDeviceChanges(device::usb::wtf::DeviceChangeNotificationPtr);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit USB(LocalFrame& frame);

    device::usb::wtf::DeviceManagerPtr m_deviceManager;
    HeapHashSet<Member<ScriptPromiseResolver>> m_deviceManagerRequests;
    device::usb::wtf::ChooserServicePtr m_chooserService;
    HeapHashSet<Member<ScriptPromiseResolver>> m_chooserServiceRequests;
};

} // namespace blink

#endif // USB_h
