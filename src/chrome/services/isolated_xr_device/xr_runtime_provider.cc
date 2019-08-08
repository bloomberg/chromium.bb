// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_runtime_provider.h"
#include "base/bind.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/common/chrome_features.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device.h"
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
#include "device/vr/oculus/oculus_device.h"
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
#include "device/vr/windows_mixed_reality/mixed_reality_device.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#endif

namespace {
// Poll for device add/remove every 5 seconds.
constexpr base::TimeDelta kTimeBetweenPollingEvents =
    base::TimeDelta::FromSecondsD(5);

void TraceHardwareAvailable(bool added, device::mojom::XRDeviceId device_id) {
  int id = static_cast<int>(device_id);
  if (added) {
    TRACE_EVENT_INSTANT1("xr", "HardwareAdded", TRACE_EVENT_SCOPE_THREAD, "id",
                         id);
  } else {
    TRACE_EVENT_INSTANT1("xr", "HardwareRemoved", TRACE_EVENT_SCOPE_THREAD,
                         "id", id);
  }
}

}  // namespace

void IsolatedXRRuntimeProvider::PollForDeviceChanges() {
  // If we have multiple devices attached, only return one.  This typically will
  // only happen if a device could be exposed through multiple APIs, such as
  // OpenVR exposing a WMR or Oculus device.
  bool disable_other_devices = false;

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (check_wmr_) {
    bool wmr_available =
        !disable_other_devices && wmr_statics_->IsHardwareAvailable();
    disable_other_devices = disable_other_devices || wmr_available;
    if (wmr_available && !wmr_device_) {
      wmr_device_ = std::make_unique<device::MixedRealityDevice>();
      TraceHardwareAvailable(true, wmr_device_->GetId());
      client_->OnDeviceAdded(
          wmr_device_->BindXRRuntimePtr(), wmr_device_->BindGamepadFactory(),
          wmr_device_->BindCompositorHost(), wmr_device_->GetId());
    } else if (wmr_device_ && !wmr_available) {
      TraceHardwareAvailable(false, wmr_device_->GetId());
      client_->OnDeviceRemoved(wmr_device_->GetId());
      wmr_device_ = nullptr;
    }
  }
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (check_oculus_) {
    bool oculus_available =
        !disable_other_devices &&
        ((oculus_device_ && oculus_device_->IsAvailable()) ||
         device::OculusDevice::IsHwAvailable());
    disable_other_devices = disable_other_devices || oculus_available;
    if (oculus_available && !oculus_device_) {
      oculus_device_ = std::make_unique<device::OculusDevice>();
      TraceHardwareAvailable(true, oculus_device_->GetId());
      client_->OnDeviceAdded(oculus_device_->BindXRRuntimePtr(),
                             oculus_device_->BindGamepadFactory(),
                             oculus_device_->BindCompositorHost(),
                             oculus_device_->GetId());
    } else if (oculus_device_ && !oculus_available) {
      TraceHardwareAvailable(false, oculus_device_->GetId());
      client_->OnDeviceRemoved(oculus_device_->GetId());
      oculus_device_ = nullptr;
    }
  }
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (check_openvr_) {
    bool openvr_available =
        !disable_other_devices &&
        ((openvr_device_ && openvr_device_->IsAvailable()) ||
         device::OpenVRDevice::IsHwAvailable());
    disable_other_devices = disable_other_devices || openvr_available;
    if (openvr_available && !openvr_device_) {
      openvr_device_ = std::make_unique<device::OpenVRDevice>();
      TraceHardwareAvailable(true, openvr_device_->GetId());
      client_->OnDeviceAdded(openvr_device_->BindXRRuntimePtr(),
                             openvr_device_->BindGamepadFactory(),
                             openvr_device_->BindCompositorHost(),
                             openvr_device_->GetId());
    } else if (openvr_device_ && !openvr_available) {
      TraceHardwareAvailable(false, openvr_device_->GetId());
      client_->OnDeviceRemoved(openvr_device_->GetId());
      openvr_device_ = nullptr;
    }
  }
#endif

  if (check_openvr_ || check_oculus_ || check_wmr_) {
    // Post a task to do this again later.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IsolatedXRRuntimeProvider::PollForDeviceChanges,
                       weak_ptr_factory_.GetWeakPtr()),
        kTimeBetweenPollingEvents);
  }
}

void IsolatedXRRuntimeProvider::SetupPollingForDeviceChanges() {
#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (base::FeatureList::IsEnabled(features::kOculusVR))
    check_oculus_ = device::OculusDevice::IsApiAvailable();
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (base::FeatureList::IsEnabled(features::kOpenVR))
    check_openvr_ = device::OpenVRDevice::IsApiAvailable();
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (base::FeatureList::IsEnabled(features::kWindowsMixedReality)) {
    wmr_statics_ = device::MixedRealityDeviceStatics::CreateInstance();
    check_wmr_ = wmr_statics_->IsApiAvailable();
  }
#endif

  // Post a task to call back every periodically.
  if (check_openvr_ || check_oculus_ || check_wmr_) {
    PollForDeviceChanges();
  }
}

void IsolatedXRRuntimeProvider::RequestDevices(
    device::mojom::IsolatedXRRuntimeProviderClientPtr client) {
  // Start polling to detect devices being added/removed.
  client_ = std::move(client);
  SetupPollingForDeviceChanges();
  client_->OnDevicesEnumerated();
}

IsolatedXRRuntimeProvider::IsolatedXRRuntimeProvider(
    std::unique_ptr<service_manager::ServiceKeepaliveRef> service_ref)
    : service_ref_(std::move(service_ref)), weak_ptr_factory_(this) {}

IsolatedXRRuntimeProvider::~IsolatedXRRuntimeProvider() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  // Explicitly null out wmr_device_ to clean up any COM objects that depend
  // on being RoInitialized
  wmr_device_ = nullptr;
#endif
}
