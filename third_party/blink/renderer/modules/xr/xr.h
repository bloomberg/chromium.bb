// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptPromiseResolver;
class XRDevice;

class XR final : public EventTargetWithInlineData,
                 public ContextLifecycleObserver,
                 public device::mojom::blink::VRServiceClient,
                 public FocusChangedObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XR);

 public:
  static XR* Create(LocalFrame& frame) { return new XR(frame); }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange);

  ScriptPromise requestDevice(ScriptState*);

  // XRServiceClient overrides.
  void OnDisplayConnected(device::mojom::blink::VRMagicWindowProviderPtr,
                          device::mojom::blink::VRDisplayHostPtr,
                          device::mojom::blink::VRDisplayClientRequest,
                          device::mojom::blink::VRDisplayInfoPtr) override;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Trace(blink::Visitor*) override;

  // FocusChangedObserver overrides.
  void FocusedFrameChanged() override;
  bool IsFrameFocused();

 private:
  explicit XR(LocalFrame& frame);

  void OnDevicesSynced();
  void ResolveRequestDevice();
  void Dispose();

  bool devices_synced_;

  HeapVector<Member<XRDevice>> devices_;
  Member<ScriptPromiseResolver> pending_devices_resolver_;
  device::mojom::blink::VRServicePtr service_;
  mojo::Binding<device::mojom::blink::VRServiceClient> binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_
