// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_H_

#include <jni.h>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace vr {
class ArCoreSessionUtils;
}  // namespace vr

namespace device {

class ArImageTransportFactory;
class ArCoreFactory;
class ArCoreGlThread;
class MailboxToSurfaceBridge;
class MailboxToSurfaceBridgeFactory;

class COMPONENT_EXPORT(VR_ARCORE) ArCoreDevice : public VRDeviceBase {
 public:
  ArCoreDevice(
      std::unique_ptr<ArCoreFactory> arcore_factory,
      std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
      std::unique_ptr<MailboxToSurfaceBridgeFactory>
          mailbox_to_surface_bridge_factory,
      std::unique_ptr<vr::ArCoreSessionUtils> arcore_session_utils);
  ~ArCoreDevice() override;

  // VRDeviceBase implementation.
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

  base::WeakPtr<ArCoreDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                             display::Display::Rotation rotation,
                             const gfx::Size& frame_size);
  void OnDrawingSurfaceTouch(bool is_primary,
                             bool touching,
                             int32_t pointer_id,
                             const gfx::PointF& location);
  void OnDrawingSurfaceDestroyed();
  void OnSessionEnded();

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

  // Called once the GL thread is started. At this point, it doesn't
  // have a valid GL context yet.
  void OnGlThreadReady(int render_process_id,
                       int render_frame_id,
                       bool use_overlay);

  // Replies to the pending mojo RequestSession request.
  void CallDeferredRequestSessionCallback(
      base::Optional<std::unordered_set<device::mojom::XRSessionFeature>>
          enabled_features);

  // Tells the GL thread to initialize a GL context and other resources,
  // using the supplied window as a drawing surface.
  void RequestArCoreGlInitialization(gfx::AcceleratedWidget window,
                                     int rotation,
                                     const gfx::Size& size);

  // Called when the GL thread's GL context initialization completes.
  void OnArCoreGlInitializationComplete(
      base::Optional<std::unordered_set<device::mojom::XRSessionFeature>>
          enabled_features);

  void OnCreateSessionCallback(
      mojom::XRRuntime::RequestSessionCallback deferred_callback,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          enabled_features,
      mojo::PendingRemote<mojom::XRFrameDataProvider> frame_data_provider,
      mojom::VRDisplayInfoPtr display_info,
      mojo::PendingRemote<mojom::XRSessionController> session_controller,
      mojom::XRPresentationConnectionPtr presentation_connection);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<ArCoreFactory> arcore_factory_;
  std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory_;
  std::unique_ptr<MailboxToSurfaceBridgeFactory> mailbox_bridge_factory_;
  std::unique_ptr<vr::ArCoreSessionUtils> arcore_session_utils_;

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  // Encapsulates data with session lifetime.
  struct SessionState {
    SessionState();
    ~SessionState();

    std::unique_ptr<ArCoreGlThread> arcore_gl_thread_;
    bool is_arcore_gl_initialized_ = false;

    base::OnceClosure start_immersive_activity_callback_;

    // The initial requestSession triggers the initialization sequence, store
    // the callback for replying once that initialization completes. Only one
    // concurrent session is supported, other requests are rejected.
    mojom::XRRuntime::RequestSessionCallback pending_request_session_callback_;

    // Collections of features that were requested on the session.
    std::unordered_set<device::mojom::XRSessionFeature> required_features_;
    std::unordered_set<device::mojom::XRSessionFeature> optional_features_;

    // Collection of features that have been enabled on the session. Initially
    // empty, will be set only once the ArCoreGl has been initialized.
    std::unordered_set<device::mojom::XRSessionFeature> enabled_features_;

    std::vector<device::mojom::XRTrackedImagePtr> tracked_images_;
  };

  // This object is reset to initial values when ending a session. This helps
  // ensure that each session has consistent per-session state.
  std::unique_ptr<SessionState> session_state_;

  base::OnceCallback<void(bool)>
      on_request_arcore_install_or_update_result_callback_;
  base::OnceCallback<void(bool)> on_request_ar_module_result_callback_;

  // Must be last.
  base::WeakPtrFactory<ArCoreDevice> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArCoreDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_H_
