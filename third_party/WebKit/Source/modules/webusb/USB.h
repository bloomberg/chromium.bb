// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_h
#define USB_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "device/usb/public/interfaces/chooser_service.mojom-blink.h"
#include "device/usb/public/interfaces/device_manager.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScriptPromiseResolver;
class ScriptState;
class USBDevice;
class USBDeviceRequestOptions;

class USB final : public EventTargetWithInlineData,
                  public ContextLifecycleObserver,
                  public device::mojom::blink::UsbDeviceManagerClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(USB);
  USING_PRE_FINALIZER(USB, Dispose);

 public:
  static USB* Create(LocalFrame& frame) { return new USB(frame); }

  virtual ~USB();

  void Dispose();

  // USB.idl
  ScriptPromise getDevices(ScriptState*);
  ScriptPromise requestDevice(ScriptState*, const USBDeviceRequestOptions&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  USBDevice* GetOrCreateDevice(device::mojom::blink::UsbDeviceInfoPtr);

  device::mojom::blink::UsbDeviceManager* GetDeviceManager() const {
    return device_manager_.get();
  }

  void OnGetDevices(ScriptPromiseResolver*,
                    Vector<device::mojom::blink::UsbDeviceInfoPtr>);
  void OnGetPermission(ScriptPromiseResolver*,
                       device::mojom::blink::UsbDeviceInfoPtr);

  // DeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::blink::UsbDeviceInfoPtr);
  void OnDeviceRemoved(device::mojom::blink::UsbDeviceInfoPtr);

  void OnDeviceManagerConnectionError();
  void OnChooserServiceConnectionError();

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget protected overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  explicit USB(LocalFrame& frame);

  void EnsureDeviceManagerConnection();

  device::mojom::blink::UsbDeviceManagerPtr device_manager_;
  HeapHashSet<Member<ScriptPromiseResolver>> device_manager_requests_;
  device::mojom::blink::UsbChooserServicePtr chooser_service_;
  HeapHashSet<Member<ScriptPromiseResolver>> chooser_service_requests_;
  mojo::Binding<device::mojom::blink::UsbDeviceManagerClient> client_binding_;
  HeapHashMap<String, WeakMember<USBDevice>> device_cache_;
};

}  // namespace blink

#endif  // USB_h
