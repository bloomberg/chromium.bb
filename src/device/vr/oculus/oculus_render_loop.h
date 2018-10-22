// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OCULUS_RENDER_LOOP_H
#define DEVICE_VR_OCULUS_RENDER_LOOP_H

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

const int kMaxOculusRenderLoopInputId = (ovrControllerType_Remote + 1);

class OculusRenderLoop : public base::Thread,
                         mojom::XRPresentationProvider,
                         mojom::XRFrameDataProvider,
                         mojom::IsolatedXRGamepadProvider {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(bool result, mojom::XRSessionPtr)>;

  OculusRenderLoop(base::RepeatingCallback<void()> on_presentation_ended);
  ~OculusRenderLoop() override;

  void RequestSession(mojom::XRRuntimeSessionOptionsPtr options,
                      RequestSessionCallback callback);
  void ExitPresent();
  base::WeakPtr<OculusRenderLoop> GetWeakPtr();

  // IsolatedXRGamepadProvider
  void RequestUpdate(mojom::IsolatedXRGamepadProvider::RequestUpdateCallback
                         callback) override;

  // XRPresentationProvider overrides:
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) override;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::ScopedHandle texture_handle) override;
  void UpdateLayerBounds(int16_t frame_id,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;
  void GetFrameData(
      mojom::XRFrameDataProvider::GetFrameDataCallback callback) override;

  // Bind a gamepad provider on the render loop thread, so we can provide
  // updates with the latest poses used for rendering.
  void RequestGamepadProvider(mojom::IsolatedXRGamepadProviderRequest request);

 private:
  // base::Thread overrides:
  void Init() override;
  void CleanUp() override;

  void ClearPendingFrame();
  void StartOvrSession();
  void StopOvrSession();
  void CreateOvrSwapChain();
  void DestroyOvrSwapChain();

  void UpdateControllerState();

  mojom::VRPosePtr GetPose();

  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      const ovrTrackingState& tracking_state);

  device::mojom::XRInputSourceStatePtr GetTouchData(
      ovrControllerType type,
      const ovrPoseStatef& pose,
      const ovrInputState& input_state,
      ovrHandType hand);

#if defined(OS_WIN)
  D3D11TextureHelper texture_helper_;
#endif

  base::OnceCallback<void()> delayed_get_frame_data_callback_;
  bool has_outstanding_frame_ = false;

  mojom::IsolatedXRGamepadProvider::RequestUpdateCallback gamepad_callback_;

  long long ovr_frame_index_ = 0;
  int16_t next_frame_id_ = 0;
  bool is_presenting_ = false;
  gfx::RectF left_bounds_;
  gfx::RectF right_bounds_;
  gfx::Size source_size_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojom::XRPresentationClientPtr submit_client_;
  ovrSession session_ = nullptr;
  ovrGraphicsLuid luid_ = {};
  ovrPosef last_render_pose_;
  ovrTextureSwapChain texture_swap_chain_ = 0;
  double sensor_time_;
  mojo::Binding<mojom::XRPresentationProvider> presentation_binding_;
  mojo::Binding<mojom::XRFrameDataProvider> frame_data_binding_;
  bool primary_input_pressed[kMaxOculusRenderLoopInputId];
  base::RepeatingCallback<void()> on_presentation_ended_;

  mojo::Binding<mojom::IsolatedXRGamepadProvider> gamepad_provider_;

  base::WeakPtrFactory<OculusRenderLoop> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OculusRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OCULUS_RENDER_LOOP_H
