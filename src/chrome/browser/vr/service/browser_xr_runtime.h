// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_
#define CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_

#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {

class XRDeviceImpl;

// This class wraps a physical device's interfaces, and registers for events.
// There is one BrowserXRRuntime per physical device runtime.
// It manages browser-side handling of state, like which XRDeviceImpl is
// listening for device activation.
class BrowserXRRuntime : public device::mojom::XRRuntimeEventListener {
 public:
  explicit BrowserXRRuntime(device::mojom::XRRuntimePtr runtime,
                            device::mojom::VRDisplayInfoPtr info);
  ~BrowserXRRuntime() override;

  device::mojom::XRRuntime* GetRuntime() { return runtime_.get(); }

  // Methods called by XRDeviceImpl to interact with the runtime's device.
  void OnRendererDeviceAdded(XRDeviceImpl* device);
  void OnRendererDeviceRemoved(XRDeviceImpl* device);
  void ExitPresent(XRDeviceImpl* device);
  void RequestSession(XRDeviceImpl* device,
                      const device::mojom::XRRuntimeSessionOptionsPtr& options,
                      device::mojom::XRDevice::RequestSessionCallback callback);
  XRDeviceImpl* GetPresentingRendererDevice() {
    return presenting_renderer_device_;
  }
  void UpdateListeningForActivate(XRDeviceImpl* device);
  device::mojom::VRDisplayInfoPtr GetVRDisplayInfo() {
    return display_info_.Clone();
  }

 private:
  // device::XRRuntimeEventListener
  void OnDisplayInfoChanged(
      device::mojom::VRDisplayInfoPtr vr_device_info) override;
  void OnExitPresent() override;
  void OnDeviceActivated(device::mojom::VRDisplayEventReason reason,
                         base::OnceCallback<void(bool)> on_handled) override;
  void OnDeviceIdle(device::mojom::VRDisplayEventReason reason) override;

  void OnInitialDevicePropertiesReceived(
      device::mojom::VRDisplayInfoPtr display_info);
  void StopImmersiveSession();
  void OnListeningForActivate(bool is_listening);
  void OnRequestSessionResult(
      base::WeakPtr<XRDeviceImpl> device,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      device::mojom::XRDevice::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      device::mojom::XRSessionControllerPtr immersive_session_controller);

  device::mojom::XRRuntimePtr runtime_;
  device::mojom::XRSessionControllerPtr immersive_session_controller_;

  std::set<XRDeviceImpl*> renderer_device_connections_;
  device::mojom::VRDisplayInfoPtr display_info_;

  XRDeviceImpl* listening_for_activation_renderer_device_ = nullptr;
  XRDeviceImpl* presenting_renderer_device_ = nullptr;

  mojo::Binding<device::mojom::XRRuntimeEventListener> binding_;

  base::WeakPtrFactory<BrowserXRRuntime> weak_ptr_factory_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_
