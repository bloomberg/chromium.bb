// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRController_h
#define VRController_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplay.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"

#include <memory>

namespace blink {

class NavigatorVR;
class VRGetDevicesCallback;

class VRController final : public GarbageCollectedFinalized<VRController>,
                           public device::mojom::blink::VRServiceClient,
                           public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(VRController);
  WTF_MAKE_NONCOPYABLE(VRController);
  USING_PRE_FINALIZER(VRController, Dispose);

 public:
  VRController(NavigatorVR*);
  virtual ~VRController();

  void GetDisplays(ScriptPromiseResolver*);
  void SetListeningForActivate(bool);

  void OnDisplayConnected(device::mojom::blink::VRMagicWindowProviderPtr,
                          device::mojom::blink::VRDisplayHostPtr,
                          device::mojom::blink::VRDisplayClientRequest,
                          device::mojom::blink::VRDisplayInfoPtr) override;

  void FocusChanged();

  virtual void Trace(blink::Visitor*);

 private:
  void OnDisplaysSynced();
  void OnGetDisplays();

  // ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;
  void Dispose();

  Member<NavigatorVR> navigator_vr_;
  VRDisplayVector displays_;

  bool display_synced_;

  Deque<std::unique_ptr<VRGetDevicesCallback>> pending_get_devices_callbacks_;
  device::mojom::blink::VRServicePtr service_;
  mojo::Binding<device::mojom::blink::VRServiceClient> binding_;
};

}  // namespace blink

#endif  // VRController_h
