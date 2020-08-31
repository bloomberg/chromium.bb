// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_H_

#include "services/device/public/mojom/usb_manager.mojom-blink-forward.h"
#include "services/device/public/mojom/usb_manager_client.mojom-blink.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class ExceptionState;
class ScriptPromiseResolver;
class ScriptState;
class USBDevice;
class USBDeviceRequestOptions;

class USB final : public EventTargetWithInlineData,
                  public ExecutionContextLifecycleObserver,
                  public device::mojom::blink::UsbDeviceManagerClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(USB);

 public:
  explicit USB(ExecutionContext&);
  ~USB() override;

  // USB.idl
  ScriptPromise getDevices(ScriptState*, ExceptionState&);
  ScriptPromise requestDevice(ScriptState*,
                              const USBDeviceRequestOptions*,
                              ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  USBDevice* GetOrCreateDevice(device::mojom::blink::UsbDeviceInfoPtr);

  mojom::blink::WebUsbService* GetWebUsbService() const {
    return service_.get();
  }

  void OnGetDevices(ScriptPromiseResolver*,
                    Vector<device::mojom::blink::UsbDeviceInfoPtr>);
  void OnGetPermission(ScriptPromiseResolver*,
                       device::mojom::blink::UsbDeviceInfoPtr);

  // DeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::blink::UsbDeviceInfoPtr) override;
  void OnDeviceRemoved(device::mojom::blink::UsbDeviceInfoPtr) override;

  void OnServiceConnectionError();

  void Trace(Visitor*) override;

 protected:
  // EventTarget protected overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  void EnsureServiceConnection();

  bool IsContextSupported() const;
  bool IsFeatureEnabled(ReportOptions) const;

  HeapMojoRemote<mojom::blink::WebUsbService> service_;
  HeapHashSet<Member<ScriptPromiseResolver>> get_devices_requests_;
  HeapHashSet<Member<ScriptPromiseResolver>> get_permission_requests_;
  HeapMojoAssociatedReceiver<device::mojom::blink::UsbDeviceManagerClient,
                             USB,
                             HeapMojoWrapperMode::kWithoutContextObserver>
      client_receiver_;
  HeapHashMap<String, WeakMember<USBDevice>> device_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_H_
