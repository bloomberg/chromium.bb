// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_

#include <jni.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/content_settings/core/common/content_settings.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_base.h"

namespace vr {
class MailboxToSurfaceBridge;
class ArCoreJavaUtils;
}  // namespace vr

namespace content {
class WebContents;
}  // namespace content

namespace device {

class ArCoreGlThread;

class ArCoreDevice : public VRDeviceBase {
 public:
  ArCoreDevice();
  ~ArCoreDevice() override;

  // VRDeviceBase implementation.
  void PauseTracking() override;
  void ResumeTracking() override;
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  base::WeakPtr<ArCoreDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnRequestInstallSupportedArCoreCanceled();

 private:
  // VRDeviceBase implementation
  bool ShouldPauseTrackingWhenFrameDataRestricted() override;
  void OnMagicWindowFrameDataRequest(
      mojom::XRFrameDataProvider::GetFrameDataCallback callback) override;
  void RequestHitTest(
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback)
      override;

  void OnMailboxBridgeReady();
  void OnArCoreGlThreadInitialized();
  void OnRequestCameraPermissionComplete(
      bool success);

  template <typename... Args>
  static void RunCallbackOnTaskRunner(
      const scoped_refptr<base::TaskRunner>& task_runner,
      base::OnceCallback<void(Args...)> callback,
      Args... args) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::forward<Args>(args)...));
  }
  template <typename... Args>
  base::OnceCallback<void(Args...)> CreateMainThreadCallback(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(&ArCoreDevice::RunCallbackOnTaskRunner<Args...>,
                          main_thread_task_runner_, std::move(callback));
  }

  void PostTaskToGlThread(base::OnceClosure task);

  bool IsOnMainThread();

  void RequestArModule(int render_process_id,
                       int render_frame_id,
                       bool has_user_activation);
  void OnRequestArModuleResult(int render_process_id,
                               int render_frame_id,
                               bool has_user_activation,
                               bool success);
  void RequestArCoreInstallOrUpdate(int render_process_id,
                                    int render_frame_id,
                                    bool has_user_activation);
  void OnRequestArCoreInstallOrUpdateResult(int render_process_id,
                                            int render_frame_id,
                                            bool has_user_activation,
                                            bool success);
  void CallDeferredRequestSessionCallbacks(bool success);
  void RequestCameraPermission(int render_process_id,
                               int render_frame_id,
                               bool has_user_activation,
                               base::OnceCallback<void(bool)> callback);
  void OnRequestCameraPermissionResult(content::WebContents* web_contents,
                                       base::OnceCallback<void(bool)> callback,
                                       ContentSetting content_setting);
  void OnRequestAndroidCameraPermissionResult(
      base::OnceCallback<void(bool)> callback,
      bool was_android_camera_permission_granted);
  void RequestArCoreGlInitialization();
  void OnArCoreGlInitializationComplete(bool success);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<ArCoreGlThread> arcore_gl_thread_;
  std::unique_ptr<vr::ArCoreJavaUtils> arcore_java_utils_;

  bool is_arcore_gl_thread_initialized_ = false;
  bool is_arcore_gl_initialized_ = false;

  // If we get a requestSession before we are completely initialized, store a
  // callback to requesting the AR module since that is the next step that needs
  // to be taken.
  base::OnceClosure pending_request_ar_module_callback_;

  // This object is not paused when it is created. Although it is not
  // necessarily running during initialization, it is not paused. If it is
  // paused before initialization completes, then the underlying runtime will
  // not be resumed.
  bool is_paused_ = false;

  std::vector<mojom::XRRuntime::RequestSessionCallback>
      deferred_request_session_callbacks_;

  base::OnceCallback<void(bool)>
      on_request_arcore_install_or_update_result_callback_;

  // Must be last.
  base::WeakPtrFactory<ArCoreDevice> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreDevice);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
