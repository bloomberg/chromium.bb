// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "device/vr/vr_device_base.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace device {

class GvrDelegateProvider;

class DEVICE_VR_EXPORT GvrDevice : public VRDeviceBase,
                                   public mojom::XRSessionController {
 public:
  GvrDevice();
  ~GvrDevice() override;

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void PauseTracking() override;
  void ResumeTracking() override;
  void EnsureInitialized(EnsureInitializedCallback callback) override;

  void OnDisplayConfigurationChanged(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);

  void Activate(mojom::VRDisplayEventReason reason,
                base::Callback<void(bool)> on_handled);

 private:
  // VRDeviceBase
  void OnListeningForActivate(bool listening) override;
  void OnGetInlineFrameData(
      mojom::XRFrameDataProvider::GetFrameDataCallback callback) override;

  void OnStartPresentResult(mojom::XRRuntime::RequestSessionCallback callback,
                            mojom::XRSessionPtr session);

  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  void OnPresentingControllerMojoConnectionError();
  void StopPresenting();
  GvrDelegateProvider* GetGvrDelegateProvider();

  void Init(base::OnceCallback<void(bool)> on_finished);
  void OnVrModuleInstalled(base::OnceCallback<void(bool)> on_finished,
                           bool success);
  void CreateNonPresentingContext();
  void OnInitRequestSessionFinished(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback,
      bool success);

  base::android::ScopedJavaGlobalRef<jobject> non_presenting_context_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;

  bool paused_ = true;

  mojo::Binding<mojom::XRSessionController> exclusive_controller_binding_;

  base::WeakPtrFactory<GvrDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
