// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEVICE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/modules/xr/xr_session_creation_options.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class XR;
class XRFrameProvider;

class XRDevice final : public EventTargetWithInlineData,
                       public device::mojom::blink::VRDisplayClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRDevice(XR*,
           device::mojom::blink::VRMagicWindowProviderPtr,
           device::mojom::blink::VRDisplayHostPtr,
           device::mojom::blink::VRDisplayClientRequest,
           device::mojom::blink::VRDisplayInfoPtr);
  XR* xr() const { return xr_; }

  bool external() const { return is_external_; }

  ScriptPromise supportsSession(ScriptState*,
                                const XRSessionCreationOptions&) const;
  ScriptPromise requestSession(ScriptState*, const XRSessionCreationOptions&);

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // XRDisplayClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnBlur() override;
  void OnFocus() override;
  void OnActivate(device::mojom::blink::VRDisplayEventReason,
                  OnActivateCallback on_handled) override;
  void OnDeactivate(device::mojom::blink::VRDisplayEventReason) override;

  XRFrameProvider* frameProvider();

  void Dispose();

  const device::mojom::blink::VRDisplayHostPtr& xrDisplayHostPtr() const {
    return display_;
  }
  const device::mojom::blink::VRMagicWindowProviderPtr&
  xrMagicWindowProviderPtr() const {
    return magic_window_provider_;
  }
  const device::mojom::blink::VRDisplayInfoPtr& xrDisplayInfoPtr() const {
    return display_info_;
  }
  // Incremented every time display_info_ is changed, so that other objects that
  // depend on it can know when they need to update.
  unsigned int xrDisplayInfoPtrId() const { return display_info_id_; }

  void Trace(blink::Visitor*) override;

 private:
  void SetXRDisplayInfo(device::mojom::blink::VRDisplayInfoPtr);

  const char* checkSessionSupport(const XRSessionCreationOptions&) const;

  Member<XR> xr_;
  Member<XRFrameProvider> frame_provider_;
  bool is_external_;
  bool supports_exclusive_;

  device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider_;
  device::mojom::blink::VRDisplayHostPtr display_;
  device::mojom::blink::VRDisplayInfoPtr display_info_;
  unsigned int display_info_id_ = 0;

  mojo::Binding<device::mojom::blink::VRDisplayClient> display_client_binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEVICE_H_
