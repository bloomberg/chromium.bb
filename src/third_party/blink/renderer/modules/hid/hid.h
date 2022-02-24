// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class HIDDeviceFilter;
class HIDDeviceRequestOptions;
class Navigator;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT HID : public EventTargetWithInlineData,
                           public ExecutionContextLifecycleObserver,
                           public Supplement<Navigator>,
                           public device::mojom::blink::HidManagerClient,
                           public HIDDevice::ServiceInterface {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web-exposed getter for navigator.hid
  static HID* hid(Navigator&);

  explicit HID(Navigator&);
  ~HID() override;

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  // device::mojom::HidManagerClient:
  void DeviceAdded(device::mojom::blink::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(
      device::mojom::blink::HidDeviceInfoPtr device_info) override;
  void DeviceChanged(
      device::mojom::blink::HidDeviceInfoPtr device_info) override;

  // Web-exposed interfaces on hid object:
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)
  ScriptPromise getDevices(ScriptState*, ExceptionState&);
  ScriptPromise requestDevice(ScriptState*,
                              const HIDDeviceRequestOptions*,
                              ExceptionState&);

  // HIDDevice::ServiceInterface:
  void Connect(
      const String& device_guid,
      mojo::PendingRemote<device::mojom::blink::HidConnectionClient>
          connection_client,
      device::mojom::blink::HidManager::ConnectCallback callback) override;
  void Forget(device::mojom::blink::HidDeviceInfoPtr device_info,
              mojom::blink::HidService::ForgetCallback callback) override;

  // Converts a HID device `filter` into the equivalent Mojo type and returns
  // it. CheckDeviceFilterValidity must be called first.
  static mojom::blink::HidDeviceFilterPtr ConvertDeviceFilter(
      const HIDDeviceFilter& filter);

  // Checks the validity of the given HIDDeviceFilter. Returns nullopt when
  // filter is valid or an error message when the filter is invalid.
  static absl::optional<String> CheckDeviceFilterValidity(
      const HIDDeviceFilter& filter);

  void Trace(Visitor*) const override;

 protected:
  // EventTarget:
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  // Returns the HIDDevice matching |info| from |device_cache_|. If the device
  // is not in the cache, a new device is created and added to the cache.
  HIDDevice* GetOrCreateDevice(device::mojom::blink::HidDeviceInfoPtr info);

  // Opens a connection to HidService, or does nothing if the connection is
  // already open.
  void EnsureServiceConnection();

  // Closes the connection to HidService and resolves any pending promises.
  void CloseServiceConnection();

  void FinishGetDevices(ScriptPromiseResolver*,
                        Vector<device::mojom::blink::HidDeviceInfoPtr>);
  void FinishRequestDevice(ScriptPromiseResolver*,
                           Vector<device::mojom::blink::HidDeviceInfoPtr>);

  HeapMojoRemote<mojom::blink::HidService> service_;
  mojo::AssociatedReceiver<device::mojom::blink::HidManagerClient> receiver_{
      this};
  HeapHashSet<Member<ScriptPromiseResolver>> get_devices_promises_;
  HeapHashSet<Member<ScriptPromiseResolver>> request_device_promises_;
  HeapHashMap<String, WeakMember<HIDDevice>> device_cache_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_
