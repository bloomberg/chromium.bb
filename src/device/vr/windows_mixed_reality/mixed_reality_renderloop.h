// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_

#include <windows.graphics.holographic.h>
#include <windows.perception.spatial.h>

#include <wrl.h>

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_winrt_initializer.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/windows/compositor_base.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/win/window_impl.h"

namespace device {

class MixedRealityWindow;

class MixedRealityRenderLoop : public XRCompositorCommon {
 public:
  explicit MixedRealityRenderLoop(
      base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
          on_display_info_changed);
  ~MixedRealityRenderLoop() override;

 private:
  // XRCompositorCommon:
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;

  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  mojom::XRGamepadDataPtr GetNextGamepadData() override;
  void GetEnvironmentIntegrationProvider(
      mojom::XREnvironmentIntegrationProviderAssociatedRequest
          environment_provider) override;
  bool PreComposite() override;
  bool SubmitCompositedFrame() override;

  // Helpers to implement XRDeviceAbstraction.
  void InitializeOrigin();
  void InitializeSpace();
  void StartPresenting();
  void UpdateWMRDataForNextFrame();

  // Returns true if display info has changed. Does not update stage parameters.
  bool UpdateDisplayInfo();
  // Returns true if stage parameters have changed.
  bool UpdateStageParameters();

  // Helper methods for the stage.
  void ClearStageOrigin();
  void InitializeStageOrigin();
  bool EnsureStageStatics();
  void ClearStageStatics();
  void OnCurrentStageChanged();

  // Will try to update the stage bounds if the following are true:
  // 1) We have a spatial_stage.
  // 2) That spatial stage supports bounded movement.
  // 3) The current bounds array is empty.
  void EnsureStageBounds();

  std::unique_ptr<base::win::ScopedWinrtInitializer> initializer_;

  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicSpace>
      holographic_space_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference>
      spatial_stage_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
      origin_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
      stage_origin_;
  bool stage_transform_needs_updating_ = false;
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                             ISpatialLocatorAttachedFrameOfReference>
      attached_;
  bool emulated_position_ = false;
  base::Optional<gfx::Transform> last_origin_from_attached_;

  std::unique_ptr<MixedRealityWindow> window_;
  mojom::VRDisplayInfoPtr current_display_info_;
  base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
      on_display_info_changed_;

  // Per frame data:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicFrame>
      holographic_frame_;
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
      timestamp_;

  // The set of all poses for this frame (there could be multiple headsets or
  // external cameras).
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
      ABI::Windows::Graphics::Holographic::HolographicCameraPose*>>
      poses_;

  // We only support one headset at a time - this is the one pose.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCameraPose>
      pose_;
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::
                             IHolographicCameraRenderingParameters>
      rendering_params_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCamera>
      camera_;

  std::unique_ptr<MixedRealityInputHelper> input_helper_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReferenceStatics>
      stage_statics_;
  EventRegistrationToken stage_changed_token_;

  std::vector<gfx::Point3F> bounds_;
  bool bounds_updated_ = false;

  // This must be the last member
  base::WeakPtrFactory<MixedRealityRenderLoop> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MixedRealityRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_
