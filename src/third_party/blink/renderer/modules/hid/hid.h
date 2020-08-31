// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_

#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

class ExecutionContext;
class HIDDevice;
class HIDDeviceRequestOptions;
class ScriptPromiseResolver;
class ScriptState;

class HID : public EventTargetWithInlineData, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HID);

 public:
  explicit HID(ExecutionContext& context);
  ~HID() override;

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // Web-exposed interfaces:
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)
  ScriptPromise getDevices(ScriptState*, ExceptionState&);
  ScriptPromise requestDevice(ScriptState*,
                              const HIDDeviceRequestOptions*,
                              ExceptionState&);

  void Connect(const String& device_guid,
               mojo::PendingRemote<device::mojom::blink::HidConnectionClient>
                   connection_client,
               device::mojom::blink::HidManager::ConnectCallback callback);

  void Trace(Visitor*) override;

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
  void OnServiceConnectionError();
  void FinishGetDevices(ScriptPromiseResolver*,
                        Vector<device::mojom::blink::HidDeviceInfoPtr>);
  void FinishRequestDevice(ScriptPromiseResolver*,
                           Vector<device::mojom::blink::HidDeviceInfoPtr>);

  HeapMojoRemote<mojom::blink::HidService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      service_;
  HeapHashSet<Member<ScriptPromiseResolver>> get_devices_promises_;
  HeapHashSet<Member<ScriptPromiseResolver>> request_device_promises_;
  HeapHashMap<String, WeakMember<HIDDevice>> device_cache_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_
