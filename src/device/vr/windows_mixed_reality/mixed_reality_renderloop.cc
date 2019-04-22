// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"

#include <HolographicSpaceInterop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <windows.perception.h>
#include <windows.perception.spatial.h>

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

// TODO(crbug.com/941546): Remove namespaces to comply with coding standard.
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Graphics::Holographic;
using namespace ABI::Windows::Perception;
using namespace ABI::Windows::Perception::Spatial;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace WFC = ABI::Windows::Foundation::Collections;
namespace WFN = ABI::Windows::Foundation::Numerics;
namespace WInput = ABI::Windows::UI::Input::Spatial;

using ABI::Windows::Foundation::IEventHandler;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Numerics::Matrix4x4;

class MixedRealityWindow : public gfx::WindowImpl {
 public:
  MixedRealityWindow() : gfx::WindowImpl() { set_window_style(WS_OVERLAPPED); }

  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override;
};

BOOL MixedRealityWindow::ProcessWindowMessage(HWND window,
                                              UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param,
                                              LRESULT& result,
                                              DWORD msg_map_id) {
  return false;  // We don't currently handle any messages ourselves.
}

namespace {
// This enum is used in TRACE_EVENTs.  Try to keep enum values the same to make
// analysis easier across builds.
enum class ErrorLocation {
  kAcquireCurrentStage = 1,
  kStationaryReferenceCreation = 2,
  kGetTransformBetweenOrigins = 3,
  kGamepadMissingTimestamp = 4,
  kGamepadMissingOrigin = 5,
};

void TraceError(ErrorLocation location, HRESULT hr) {
  TRACE_EVENT_INSTANT2("xr", "WMRRenderLoopError", TRACE_EVENT_SCOPE_THREAD,
                       "ErrorLocation", location, "hr", hr);
}

std::vector<float> ConvertToStandingTransform(const Matrix4x4& transform) {
  return {transform.M11, transform.M12, transform.M13, transform.M14,
          transform.M21, transform.M22, transform.M23, transform.M24,
          transform.M31, transform.M32, transform.M33, transform.M34,
          transform.M41, transform.M42, transform.M43, transform.M44};
}

mojom::VRFieldOfViewPtr ParseProjection(const Matrix4x4& projection) {
  gfx::Transform proj(
      projection.M11, projection.M21, projection.M31, projection.M41,
      projection.M12, projection.M22, projection.M32, projection.M42,
      projection.M13, projection.M23, projection.M33, projection.M43,
      projection.M14, projection.M24, projection.M34, projection.M44);

  gfx::Transform projInv;
  bool invertable = proj.GetInverse(&projInv);
  DCHECK(invertable);

  // We will convert several points from projection space into view space to
  // calculate the view frustum angles.  We are assuming some common form for
  // the projection matrix.
  gfx::Point3F left_top_far(-1, 1, 1);
  gfx::Point3F left_top_near(-1, 1, 0);
  gfx::Point3F right_bottom_far(1, -1, 1);
  gfx::Point3F right_bottom_near(1, -1, 0);

  projInv.TransformPoint(&left_top_far);
  projInv.TransformPoint(&left_top_near);
  projInv.TransformPoint(&right_bottom_far);
  projInv.TransformPoint(&right_bottom_near);

  float left_on_far_plane = left_top_far.x();
  float top_on_far_plane = left_top_far.y();
  float right_on_far_plane = right_bottom_far.x();
  float bottom_on_far_plane = right_bottom_far.y();
  float far_plane = left_top_far.z();

  mojom::VRFieldOfViewPtr field_of_view = mojom::VRFieldOfView::New();
  field_of_view->upDegrees =
      gfx::RadToDeg(atanf(-top_on_far_plane / far_plane));
  field_of_view->downDegrees =
      gfx::RadToDeg(atanf(bottom_on_far_plane / far_plane));
  field_of_view->leftDegrees =
      gfx::RadToDeg(atanf(left_on_far_plane / far_plane));
  field_of_view->rightDegrees =
      gfx::RadToDeg(atanf(-right_on_far_plane / far_plane));

  // TODO(billorr): Expand the mojo interface to support just sending the
  // projection matrix directly, instead of decomposing it.
  return field_of_view;
}
}  // namespace

MixedRealityRenderLoop::MixedRealityRenderLoop(
    base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
        on_display_info_changed)
    : XRCompositorCommon(),
      on_display_info_changed_(std::move(on_display_info_changed)),
      weak_ptr_factory_(this) {
  stage_changed_token_.value = 0;
}

MixedRealityRenderLoop::~MixedRealityRenderLoop() {
  Stop();
}

bool MixedRealityRenderLoop::PreComposite() {
  if (rendering_params_) {
    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>
        surface;
    if (FAILED(rendering_params_->get_Direct3D11BackBuffer(&surface)))
      return false;
    Microsoft::WRL::ComPtr<
        Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>
        dxgi_interface_access;
    if (FAILED(surface.As(&dxgi_interface_access)))
      return false;
    Microsoft::WRL::ComPtr<ID3D11Resource> native_resource;
    if (FAILED(dxgi_interface_access->GetInterface(
            IID_PPV_ARGS(&native_resource))))
      return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (FAILED(native_resource.As(&texture)))
      return false;
    texture_helper_.SetBackbuffer(texture);
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    ABI::Windows::Foundation::Rect viewport;
    if (FAILED(pose_->get_Viewport(&viewport)))
      return false;

    gfx::RectF override_viewport =
        gfx::RectF(viewport.X / desc.Width, viewport.Y / desc.Height,
                   viewport.Width / desc.Width, viewport.Height / desc.Height);

    texture_helper_.OverrideViewports(override_viewport, override_viewport);
    texture_helper_.SetDefaultSize(gfx::Size(desc.Width, desc.Height));

    TRACE_EVENT_INSTANT0("xr", "PreCompositorWMR", TRACE_EVENT_SCOPE_THREAD);
  }
  return true;
}

bool MixedRealityRenderLoop::SubmitCompositedFrame() {
  ABI::Windows::Graphics::Holographic::HolographicFramePresentResult result =
      HolographicFramePresentResult_Success;
  HRESULT hr = holographic_frame_->PresentUsingCurrentPrediction(&result);

  TRACE_EVENT_INSTANT2("xr", "SubmitWMR", TRACE_EVENT_SCOPE_THREAD, "hr", hr,
                       "result", result);

  if (FAILED(hr))
    return false;

  return true;
}

namespace {

FARPROC LoadD3D11Function(const char* function_name) {
  static HMODULE const handle = ::LoadLibrary(L"d3d11.dll");
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::CreateDirect3D11DeviceFromDXGIDevice)
GetCreateDirect3D11DeviceFromDXGIDeviceFunction() {
  static decltype(&::CreateDirect3D11DeviceFromDXGIDevice) const function =
      reinterpret_cast<decltype(&::CreateDirect3D11DeviceFromDXGIDevice)>(
          LoadD3D11Function("CreateDirect3D11DeviceFromDXGIDevice"));
  return function;
}

HRESULT WrapperCreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice* in,
                                                    IInspectable** out) {
  *out = nullptr;
  auto func = GetCreateDirect3D11DeviceFromDXGIDeviceFunction();
  if (!func)
    return E_FAIL;
  return func(in, out);
}

}  // namespace

bool MixedRealityRenderLoop::StartRuntime() {
  initializer_ = std::make_unique<base::win::ScopedWinrtInitializer>();

  InitializeSpace();
  if (!holographic_space_)
    return false;

  input_helper_ = std::make_unique<MixedRealityInputHelper>(window_->hwnd());

  ABI::Windows::Graphics::Holographic::HolographicAdapterId id;
  HRESULT hr = holographic_space_->get_PrimaryAdapterId(&id);
  if (FAILED(hr))
    return false;

  LUID adapter_luid;
  adapter_luid.HighPart = id.HighPart;
  adapter_luid.LowPart = id.LowPart;
  texture_helper_.SetUseBGRA(true);
  if (!texture_helper_.SetAdapterLUID(adapter_luid) ||
      !texture_helper_.EnsureInitialized()) {
    return false;
  }

  // Associate our holographic space with our directx device.
  ComPtr<IDXGIDevice> dxgi_device;
  hr = texture_helper_.GetDevice().As(&dxgi_device);
  if (FAILED(hr))
    return false;

  ComPtr<IInspectable> spInsp;
  hr = WrapperCreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), &spInsp);
  if (FAILED(hr))
    return false;

  ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> device;
  hr = spInsp.As(&device);
  if (FAILED(hr))
    return false;

  hr = holographic_space_->SetDirect3D11Device(device.Get());
  return SUCCEEDED(hr);
}

void MixedRealityRenderLoop::StopRuntime() {
  if (window_)
    ShowWindow(window_->hwnd(), SW_HIDE);
  holographic_space_ = nullptr;
  origin_ = nullptr;
  last_origin_from_attached_ = base::nullopt;
  attached_ = nullptr;
  ClearStageStatics();
  ClearStageOrigin();

  holographic_frame_ = nullptr;
  timestamp_ = nullptr;
  poses_ = nullptr;
  pose_ = nullptr;
  rendering_params_ = nullptr;
  camera_ = nullptr;

  if (input_helper_)
    input_helper_->Dispose();
  input_helper_ = nullptr;

  if (window_)
    DestroyWindow(window_->hwnd());
  window_ = nullptr;

  if (initializer_)
    initializer_ = nullptr;
}

void MixedRealityRenderLoop::InitializeOrigin() {
  TRACE_EVENT0("xr", "InitializeOrigin");

  stage_transform_needs_updating_ = true;

  // Try to get a stationary frame.  We'll hand out all of our poses in this
  // space.
  ComPtr<ISpatialLocatorStatics> spatial_locator_statics;
  base::win::ScopedHString spatial_locator_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialLocator);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_locator_string.get(), IID_PPV_ARGS(&spatial_locator_statics));
  if (FAILED(hr))
    return;

  ComPtr<ISpatialLocator> locator;
  hr = spatial_locator_statics->GetDefault(&locator);
  if (FAILED(hr))
    return;

  if (!attached_) {
    hr = locator->CreateAttachedFrameOfReferenceAtCurrentHeading(&attached_);
    if (FAILED(hr))
      return;
  }

  ComPtr<ISpatialStationaryFrameOfReference> stationary_frame;
  hr = locator->CreateStationaryFrameOfReferenceAtCurrentLocation(
      &stationary_frame);
  if (FAILED(hr)) {
    TraceError(ErrorLocation::kStationaryReferenceCreation, hr);
    return;
  }

  hr = stationary_frame->get_CoordinateSystem(&origin_);
  if (FAILED(hr))
    return;
}

void MixedRealityRenderLoop::ClearStageOrigin() {
  stage_origin_ = nullptr;
  spatial_stage_ = nullptr;
  bounds_.clear();
  bounds_updated_ = true;
  stage_transform_needs_updating_ = true;
}

void MixedRealityRenderLoop::InitializeStageOrigin() {
  TRACE_EVENT0("xr", "InitializeStageOrigin");
  if (!EnsureStageStatics())
    return;
  stage_transform_needs_updating_ = true;

  // Try to get a SpatialStageFrameOfReference.  We'll use this to calculate
  // the transform between the poses we're handing out and where the floor is.
  HRESULT hr = stage_statics_->get_Current(&spatial_stage_);
  if (FAILED(hr) || !spatial_stage_) {
    TraceError(ErrorLocation::kAcquireCurrentStage, hr);
    return;
  }

  hr = spatial_stage_->get_CoordinateSystem(&stage_origin_);
  if (FAILED(hr))
    return;

  EnsureStageBounds();
}

bool MixedRealityRenderLoop::EnsureStageStatics() {
  if (stage_statics_)
    return true;

  base::win::ScopedHString spatial_stage_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialStageFrameOfReference);
  HRESULT hr = base::win::RoGetActivationFactory(spatial_stage_string.get(),
                                                 IID_PPV_ARGS(&stage_statics_));
  if (FAILED(hr))
    return false;

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  scoped_refptr<base::SingleThreadTaskRunner> thread_runner = task_runner();

  auto callback = Microsoft::WRL::Callback<IEventHandler<IInspectable*>>(
      [weak_this, thread_runner](IInspectable*, IInspectable*) {
        thread_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&MixedRealityRenderLoop::OnCurrentStageChanged,
                           weak_this));
        return S_OK;
      });

  DCHECK(stage_changed_token_.value == 0);
  hr =
      stage_statics_->add_CurrentChanged(callback.Get(), &stage_changed_token_);
  DCHECK(SUCCEEDED(hr));

  return true;
}

void MixedRealityRenderLoop::ClearStageStatics() {
  if (!stage_statics_)
    return;

  HRESULT hr = S_OK;
  if (stage_changed_token_.value != 0) {
    hr = stage_statics_->remove_CurrentChanged(stage_changed_token_);
    stage_changed_token_.value = 0;
    DCHECK(SUCCEEDED(hr));
  }

  stage_statics_ = nullptr;
}

void MixedRealityRenderLoop::OnCurrentStageChanged() {
  stage_origin_ = nullptr;
  InitializeStageOrigin();
}

void MixedRealityRenderLoop::EnsureStageBounds() {
  if (!spatial_stage_)
    return;

  SpatialMovementRange movement_range;
  HRESULT hr = spatial_stage_->get_MovementRange(&movement_range);
  DCHECK(SUCCEEDED(hr));
  if (movement_range != SpatialMovementRange::SpatialMovementRange_Bounded)
    return;

  if (bounds_.size() != 0)
    return;

  if (!stage_origin_)
    return;

  uint32_t size;
  base::win::ScopedCoMem<WFN::Vector3> bounds;
  hr =
      spatial_stage_->TryGetMovementBounds(stage_origin_.Get(), &size, &bounds);
  if (FAILED(hr))
    return;

  if (size == 0)
    return;

  // TryGetMovementBounds gives us the points in clockwise order, so we don't
  // need to reverse their order here.
  for (uint32_t i = 0; i < size; i++) {
    WFN::Vector3 val = bounds[i];
    bounds_.emplace_back(val.X, val.Y, val.Z);
  }

  bounds_updated_ = true;
}

void MixedRealityRenderLoop::OnSessionStart() {
  // Each session should start with new origins.
  origin_ = nullptr;
  attached_ = nullptr;
  last_origin_from_attached_ = base::nullopt;
  InitializeOrigin();

  ClearStageOrigin();
  InitializeStageOrigin();

  StartPresenting();
}

void MixedRealityRenderLoop::InitializeSpace() {
  // Create a Window, which is required to get an IHolographicSpace.
  window_ = std::make_unique<MixedRealityWindow>();

  // A small arbitrary size that keeps the window from being distracting.
  window_->Init(NULL, gfx::Rect(25, 10));

  // Create a holographic space from that Window.
  ComPtr<IHolographicSpaceInterop> holographic_space_interop;
  base::win::ScopedHString holographic_space_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace);
  HRESULT hr = base::win::RoGetActivationFactory(
      holographic_space_string.get(), IID_PPV_ARGS(&holographic_space_interop));

  if (SUCCEEDED(hr)) {
    hr = holographic_space_interop->CreateForWindow(
        window_->hwnd(), IID_PPV_ARGS(&holographic_space_));
  }
}

void MixedRealityRenderLoop::StartPresenting() {
  ShowWindow(window_->hwnd(), SW_SHOW);
}

mojom::XRGamepadDataPtr MixedRealityRenderLoop::GetNextGamepadData() {
  if (!timestamp_) {
    TraceError(ErrorLocation::kGamepadMissingTimestamp, E_UNEXPECTED);
    return nullptr;
  }

  if (!origin_) {
    TraceError(ErrorLocation::kGamepadMissingOrigin, E_UNEXPECTED);
    return nullptr;
  }

  return input_helper_->GetWebVRGamepadData(origin_, timestamp_);
}

struct EyeToWorldDecomposed {
  gfx::Quaternion world_to_eye_rotation;
  gfx::Point3F eye_in_world_space;
};

EyeToWorldDecomposed DecomposeViewMatrix(
    const ABI::Windows::Foundation::Numerics::Matrix4x4& view) {
  gfx::Transform world_to_view(view.M11, view.M21, view.M31, view.M41, view.M12,
                               view.M22, view.M32, view.M42, view.M13, view.M23,
                               view.M33, view.M43, view.M14, view.M24, view.M34,
                               view.M44);

  gfx::Transform view_to_world;
  bool invertable = world_to_view.GetInverse(&view_to_world);
  DCHECK(invertable);

  gfx::Point3F eye_in_world_space(view_to_world.matrix().get(0, 3),
                                  view_to_world.matrix().get(1, 3),
                                  view_to_world.matrix().get(2, 3));

  gfx::DecomposedTransform world_to_view_decomposed;
  bool decomposable =
      gfx::DecomposeTransform(&world_to_view_decomposed, world_to_view);
  DCHECK(decomposable);

  gfx::Quaternion world_to_eye_rotation = world_to_view_decomposed.quaternion;
  return {world_to_eye_rotation.inverse(), eye_in_world_space};
}

mojom::VRPosePtr GetMonoViewData(const HolographicStereoTransform& view) {
  auto eye = DecomposeViewMatrix(view.Left);

  auto pose = mojom::VRPose::New();

  // World to device orientation.
  pose->orientation =
      std::vector<float>{static_cast<float>(eye.world_to_eye_rotation.x()),
                         static_cast<float>(eye.world_to_eye_rotation.y()),
                         static_cast<float>(eye.world_to_eye_rotation.z()),
                         static_cast<float>(eye.world_to_eye_rotation.w())};

  // Position in world space.
  pose->position =
      std::vector<float>{eye.eye_in_world_space.x(), eye.eye_in_world_space.y(),
                         eye.eye_in_world_space.z()};

  return pose;
}

struct PoseAndEyeTransform {
  mojom::VRPosePtr pose;
  std::vector<float> left_offset;
  std::vector<float> right_offset;
};

PoseAndEyeTransform GetStereoViewData(const HolographicStereoTransform& view) {
  auto left_eye = DecomposeViewMatrix(view.Left);
  auto right_eye = DecomposeViewMatrix(view.Right);
  auto center = gfx::Point3F(
      (left_eye.eye_in_world_space.x() + right_eye.eye_in_world_space.x()) / 2,
      (left_eye.eye_in_world_space.y() + right_eye.eye_in_world_space.y()) / 2,
      (left_eye.eye_in_world_space.z() + right_eye.eye_in_world_space.z()) / 2);

  // We calculate the overal headset pose to be the slerp of per-eye poses as
  // calculated by the view transform's decompositions.  Although this works, we
  // should consider using per-eye rotation as well as translation for eye
  // parameters. See https://crbug.com/928433 for a similar issue.
  gfx::Quaternion world_to_view_rotation = left_eye.world_to_eye_rotation;
  world_to_view_rotation.Slerp(right_eye.world_to_eye_rotation, 0.5f);

  // Calculate new eye offsets.
  gfx::Vector3dF left_offset = left_eye.eye_in_world_space - center;
  gfx::Vector3dF right_offset = right_eye.eye_in_world_space - center;

  gfx::Transform transform(world_to_view_rotation);  // World to view.
  transform.Transpose();                             // Now it is view to world.

  transform.TransformVector(&left_offset);  // Offset is now in view space
  transform.TransformVector(&right_offset);

  PoseAndEyeTransform ret;
  ret.right_offset =
      std::vector<float>{right_offset.x(), right_offset.y(), right_offset.z()};
  ret.left_offset =
      std::vector<float>{left_offset.x(), left_offset.y(), left_offset.z()};

  // TODO(https://crbug.com/928433): We don't currently support per-eye rotation
  // in the mojo interface, but we should.

  ret.pose = mojom::VRPose::New();

  // World to device orientation.
  ret.pose->orientation =
      std::vector<float>{static_cast<float>(world_to_view_rotation.x()),
                         static_cast<float>(world_to_view_rotation.y()),
                         static_cast<float>(world_to_view_rotation.z()),
                         static_cast<float>(world_to_view_rotation.w())};

  // Position in world space.
  ret.pose->position = std::vector<float>{center.x(), center.y(), center.z()};

  return ret;
}

mojom::XRFrameDataPtr CreateDefaultFrameData(
    ComPtr<IPerceptionTimestamp> timestamp,
    int16_t frame_id) {
  mojom::XRFrameDataPtr ret = mojom::XRFrameData::New();

  ABI::Windows::Foundation::DateTime date_time;
  HRESULT hr = timestamp->get_TargetTime(&date_time);
  DCHECK(SUCCEEDED(hr));

  ABI::Windows::Foundation::TimeSpan relative_time;
  if (SUCCEEDED(timestamp->get_PredictionAmount(&relative_time))) {
    // relative_time.Duration is a count of 100ns units, so multiply by 100
    // to get a count of nanoseconds.
    double milliseconds =
        base::TimeDelta::FromNanosecondsD(100.0 * relative_time.Duration)
            .InMillisecondsF();
    TRACE_EVENT_INSTANT1("gpu", "WebXR pose prediction",
                         TRACE_EVENT_SCOPE_THREAD, "milliseconds",
                         milliseconds);
  }

  ret->time_delta =
      base::TimeDelta::FromMicroseconds(date_time.UniversalTime / 10);
  ret->frame_id = frame_id;
  return ret;
}

void MixedRealityRenderLoop::UpdateWMRDataForNextFrame() {
  holographic_frame_ = nullptr;
  poses_ = nullptr;
  pose_ = nullptr;
  rendering_params_ = nullptr;
  camera_ = nullptr;
  timestamp_ = nullptr;

  // Start populating this frame's data.
  HRESULT hr = holographic_space_->CreateNextFrame(&holographic_frame_);
  if (FAILED(hr))
    return;

  ComPtr<IHolographicFramePrediction> prediction;
  hr = holographic_frame_->get_CurrentPrediction(&prediction);
  if (FAILED(hr))
    return;

  hr = prediction->get_Timestamp(&timestamp_);
  if (FAILED(hr))
    return;

  hr = prediction->get_CameraPoses(&poses_);
  if (FAILED(hr))
    return;

  unsigned int num;
  hr = poses_->get_Size(&num);
  if (FAILED(hr) || num != 1) {
    return;
  }

  // There is only 1 pose.
  hr = poses_->GetAt(0, &pose_);
  if (FAILED(hr))
    return;

  hr = holographic_frame_->GetRenderingParameters(pose_.Get(),
                                                  &rendering_params_);
  if (FAILED(hr))
    return;

  // Make sure we have an origin.
  if (!origin_) {
    InitializeOrigin();
  }

  // Make sure we have a stage origin.
  if (!stage_origin_)
    InitializeStageOrigin();

  if (FAILED(pose_->get_HolographicCamera(&camera_)))
    return;
}

bool MixedRealityRenderLoop::UpdateDisplayInfo() {
  if (!pose_)
    return false;
  if (!camera_)
    return false;

  ABI::Windows::Graphics::Holographic::HolographicStereoTransform projection;
  if (FAILED(pose_->get_ProjectionTransform(&projection)))
    return false;

  ABI::Windows::Foundation::Size size;
  if (FAILED(camera_->get_RenderTargetSize(&size)))
    return false;
  boolean stereo;
  if (FAILED(camera_->get_IsStereo(&stereo)))
    return false;

  bool changed = false;

  if (!current_display_info_) {
    current_display_info_ = mojom::VRDisplayInfo::New();
    current_display_info_->id =
        device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID;
    current_display_info_->displayName =
        "Windows Mixed Reality";  // TODO(billorr): share this string.
    current_display_info_->capabilities = mojom::VRDisplayCapabilities::New(
        true /* hasPosition */, true /* hasExternalDisplay */,
        true /* canPresent */, false /* canProvideEnvironmentIntegration */);

    // TODO(billorr): consider scaling framebuffers after rendering support is
    // added.
    current_display_info_->webvr_default_framebuffer_scale = 1.0f;
    current_display_info_->webxr_default_framebuffer_scale = 1.0f;

    changed = true;
  }

  if (!stereo && current_display_info_->rightEye) {
    changed = true;
    current_display_info_->rightEye = nullptr;
  }

  if (!current_display_info_->leftEye) {
    current_display_info_->leftEye = mojom::VREyeParameters::New();
    changed = true;
  }

  if (current_display_info_->leftEye->renderWidth != size.Width ||
      current_display_info_->leftEye->renderHeight != size.Height) {
    changed = true;
    current_display_info_->leftEye->renderWidth = size.Width;
    current_display_info_->leftEye->renderHeight = size.Height;
  }

  auto left_fov = ParseProjection(projection.Left);
  if (!current_display_info_->leftEye->fieldOfView ||
      !left_fov->Equals(*current_display_info_->leftEye->fieldOfView)) {
    current_display_info_->leftEye->fieldOfView = std::move(left_fov);
    changed = true;
  }

  if (stereo) {
    if (!current_display_info_->rightEye) {
      current_display_info_->rightEye = mojom::VREyeParameters::New();
      changed = true;
    }

    if (current_display_info_->rightEye->renderWidth != size.Width ||
        current_display_info_->rightEye->renderHeight != size.Height) {
      changed = true;
      current_display_info_->rightEye->renderWidth = size.Width;
      current_display_info_->rightEye->renderHeight = size.Height;
    }

    auto right_fov = ParseProjection(projection.Right);
    if (!current_display_info_->rightEye->fieldOfView ||
        !right_fov->Equals(*current_display_info_->rightEye->fieldOfView)) {
      current_display_info_->rightEye->fieldOfView = std::move(right_fov);
      changed = true;
    }
  }

  return changed;
}

bool MixedRealityRenderLoop::UpdateStageParameters() {
  // TODO(https://crbug.com/945408): We should consider subscribing to
  // SpatialStageFrameOfReference.CurrentChanged to also re-calculate this.
  bool changed = false;
  if (stage_transform_needs_updating_) {
    if (!(stage_origin_ && origin_) && current_display_info_->stageParameters) {
      changed = true;
      current_display_info_->stageParameters = nullptr;
    } else if (stage_origin_ && origin_) {
      changed = true;
      current_display_info_->stageParameters = nullptr;

      mojom::VRStageParametersPtr stage_parameters =
          mojom::VRStageParameters::New();

      ComPtr<IReference<Matrix4x4>> origin_to_stage_ref;
      HRESULT hr =
          origin_->TryGetTransformTo(stage_origin_.Get(), &origin_to_stage_ref);
      if (FAILED(hr) || !origin_to_stage_ref) {
        TraceError(ErrorLocation::kGetTransformBetweenOrigins, hr);

        // We failed to get a transform between the two, so force a
        // recalculation of the stage origin and leave the stageParameters null.
        ClearStageOrigin();
        return changed;
      }

      Matrix4x4 origin_to_stage;
      hr = origin_to_stage_ref->get_Value(&origin_to_stage);
      DCHECK(SUCCEEDED(hr));

      stage_parameters->standingTransform =
          ConvertToStandingTransform(origin_to_stage);

      current_display_info_->stageParameters = std::move(stage_parameters);
    }

    stage_transform_needs_updating_ = false;
  }

  EnsureStageBounds();
  if (bounds_updated_ && current_display_info_->stageParameters) {
    current_display_info_->stageParameters->bounds = bounds_;
    changed = true;
    bounds_updated_ = false;
  }
  return changed;
}

mojom::XRFrameDataPtr MixedRealityRenderLoop::GetNextFrameData() {
  UpdateWMRDataForNextFrame();
  if (!timestamp_) {
    TRACE_EVENT_INSTANT0("xr", "No Timestamp", TRACE_EVENT_SCOPE_THREAD);
    return nullptr;
  }

  // Once we have a prediction, we can generate a frame data.
  mojom::XRFrameDataPtr ret =
      CreateDefaultFrameData(timestamp_, next_frame_id_);

  if ((!attached_ && !origin_) || !pose_) {
    TRACE_EVENT_INSTANT0("xr", "No origin or no pose",
                         TRACE_EVENT_SCOPE_THREAD);
    // If we don't have an origin or pose for this frame, we can still give out
    // a timestamp and frame to render head-locked content.
    return ret;
  }

  ComPtr<ISpatialCoordinateSystem> attached_coordinates;
  HRESULT hr = attached_->GetStationaryCoordinateSystemAtTimestamp(
      timestamp_.Get(), &attached_coordinates);
  if (FAILED(hr))
    return ret;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IReference<
      ABI::Windows::Graphics::Holographic::HolographicStereoTransform>>
      view_ref;
  if (origin_ &&
      SUCCEEDED(pose_->TryGetViewTransform(origin_.Get(), &view_ref)) &&
      view_ref) {
    // TODO(http://crbug.com/931393): Send down emulated_position_, and report
    // reset events when this changes.
    emulated_position_ = false;
    ComPtr<IReference<ABI::Windows::Foundation::Numerics::Matrix4x4>>
        attached_to_origin_ref;
    hr = attached_coordinates->TryGetTransformTo(origin_.Get(),
                                                 &attached_to_origin_ref);
    if (SUCCEEDED(hr) && attached_to_origin_ref) {
      ABI::Windows::Foundation::Numerics::Matrix4x4 transform;
      hr = attached_to_origin_ref->get_Value(&transform);
      DCHECK(SUCCEEDED(hr));
      last_origin_from_attached_ = gfx::Transform(
          transform.M11, transform.M21, transform.M31, transform.M41,
          transform.M12, transform.M22, transform.M32, transform.M42,
          transform.M13, transform.M23, transform.M33, transform.M43,
          transform.M14, transform.M24, transform.M34, transform.M44);
    }
  } else {
    emulated_position_ = true;
    if (FAILED(pose_->TryGetViewTransform(attached_coordinates.Get(),
                                          &view_ref)) ||
        !view_ref) {
      TRACE_EVENT_INSTANT0("xr", "Failed to locate origin",
                           TRACE_EVENT_SCOPE_THREAD);
      return ret;
    }
  }

  ABI::Windows::Graphics::Holographic::HolographicStereoTransform view;
  if (FAILED(view_ref->get_Value(&view))) {
    TRACE_EVENT_INSTANT0("xr", "No view transform", TRACE_EVENT_SCOPE_THREAD);
    return ret;
  }

  bool send_new_display_info = UpdateDisplayInfo();
  if (!current_display_info_) {
    TRACE_EVENT_INSTANT0("xr", "No display info", TRACE_EVENT_SCOPE_THREAD);
    return ret;
  }

  if (current_display_info_->rightEye) {
    // If we have a right eye, we are stereo.
    PoseAndEyeTransform pose_and_eye_transform = GetStereoViewData(view);
    ret->pose = std::move(pose_and_eye_transform.pose);

    if (current_display_info_->leftEye->offset !=
            pose_and_eye_transform.left_offset ||
        current_display_info_->rightEye->offset !=
            pose_and_eye_transform.right_offset) {
      current_display_info_->leftEye->offset =
          std::move(pose_and_eye_transform.left_offset);
      current_display_info_->rightEye->offset =
          std::move(pose_and_eye_transform.right_offset);
      send_new_display_info = true;
    }
  } else {
    ret->pose = GetMonoViewData(view);
    std::vector<float> offset = {0, 0, 0};
    if (current_display_info_->leftEye->offset != offset) {
      current_display_info_->leftEye->offset = offset;
      send_new_display_info = true;
    }
  }

  // The only display info we've updated so far is the eye info.
  if (send_new_display_info) {
    // Update the eye info for this frame.
    ret->left_eye = current_display_info_->leftEye.Clone();
    ret->right_eye = current_display_info_->rightEye.Clone();
  }

  bool stage_parameters_updated = UpdateStageParameters();
  if (stage_parameters_updated) {
    ret->stage_parameters_updated = true;
    ret->stage_parameters = current_display_info_->stageParameters.Clone();
  }

  if (send_new_display_info || stage_parameters_updated) {
    // Notify the device about the display info change.
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_display_info_changed_,
                                  current_display_info_.Clone()));
  }

  ret->pose->input_state = input_helper_->GetInputState(origin_, timestamp_);

  if (emulated_position_ && last_origin_from_attached_) {
    gfx::DecomposedTransform attached_from_view_decomp;
    attached_from_view_decomp.quaternion = gfx::Quaternion(
        (*ret->pose->orientation)[0], (*ret->pose->orientation)[1],
        (*ret->pose->orientation)[2], (*ret->pose->orientation)[3]);
    for (int i = 0; i < 3; ++i) {
      attached_from_view_decomp.translate[i] = (*ret->pose->position)[i];
    }
    gfx::Transform attached_from_view =
        gfx::ComposeTransform(attached_from_view_decomp);
    gfx::Transform origin_from_view =
        (*last_origin_from_attached_) * attached_from_view;
    gfx::DecomposedTransform origin_from_view_decomposed;
    bool success =
        gfx::DecomposeTransform(&origin_from_view_decomposed, origin_from_view);
    DCHECK(success);
    ret->pose->orientation = std::vector<float>{
        static_cast<float>(origin_from_view_decomposed.quaternion.x()),
        static_cast<float>(origin_from_view_decomposed.quaternion.y()),
        static_cast<float>(origin_from_view_decomposed.quaternion.z()),
        static_cast<float>(origin_from_view_decomposed.quaternion.w())};
    ret->pose->position = std::vector<float>{
        static_cast<float>(origin_from_view_decomposed.translate[0]),
        static_cast<float>(origin_from_view_decomposed.translate[1]),
        static_cast<float>(origin_from_view_decomposed.translate[2])};
  }

  return ret;
}

void MixedRealityRenderLoop::GetEnvironmentIntegrationProvider(
    mojom::XREnvironmentIntegrationProviderAssociatedRequest
        environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
  return;
}

}  // namespace device
