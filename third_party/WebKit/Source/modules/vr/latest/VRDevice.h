// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDevice_h
#define VRDevice_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/events/EventTarget.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/latest/VRSessionCreationOptions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class VR;

class VRDevice final : public EventTargetWithInlineData,
                       public device::mojom::blink::VRDisplayClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRDevice(VR*,
           device::mojom::blink::VRMagicWindowProviderPtr,
           device::mojom::blink::VRDisplayHostPtr,
           device::mojom::blink::VRDisplayClientRequest,
           device::mojom::blink::VRDisplayInfoPtr);
  VR* vr() const { return vr_; }

  const String& deviceName() const { return device_name_; }
  bool isExternal() const { return is_external_; }

  ScriptPromise supportsSession(ScriptState*,
                                const VRSessionCreationOptions&) const;
  ScriptPromise requestSession(ScriptState*, const VRSessionCreationOptions&);

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // VRDisplayClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnBlur() override;
  void OnFocus() override;
  void OnActivate(device::mojom::blink::VRDisplayEventReason,
                  OnActivateCallback on_handled) override;
  void OnDeactivate(device::mojom::blink::VRDisplayEventReason) override;

  void Dispose();

  const device::mojom::blink::VRDisplayHostPtr& vrDisplayHostPtr() const {
    return display_;
  }
  const device::mojom::blink::VRDisplayInfoPtr& vrDisplayInfoPtr() const {
    return display_info_;
  }

  virtual void Trace(blink::Visitor*);

 private:
  void SetVRDisplayInfo(device::mojom::blink::VRDisplayInfoPtr);

  const char* checkSessionSupport(const VRSessionCreationOptions&) const;

  Member<VR> vr_;
  String device_name_;
  bool is_external_;
  bool supports_exclusive_;

  device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider_;
  device::mojom::blink::VRDisplayHostPtr display_;
  device::mojom::blink::VRDisplayInfoPtr display_info_;

  mojo::Binding<device::mojom::blink::VRDisplayClient> display_client_binding_;
};

}  // namespace blink

#endif  // VRDevice_h
