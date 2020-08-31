// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_api_wrapper.h"

#include <stdint.h>
#include <algorithm>
#include <array>

#include "base/check.h"
#include "base/notreached.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/test/test_hook.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {
constexpr XrSystemId kInvalidSystem = -1;
// Only supported view configuration:
constexpr XrViewConfigurationType kSupportedViewConfiguration =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
constexpr uint32_t kNumViews = 2;

// We can get into a state where frames are not requested, such as when the
// visibility state is hidden. Since OpenXR events are polled at the beginning
// of a frame, polling would not occur in this state. To ensure events are
// occasionally polled, a timer loop run every kTimeBetweenPollingEvents to poll
// events if significant time has elapsed since the last time events were
// polled.
constexpr base::TimeDelta kTimeBetweenPollingEvents =
    base::TimeDelta::FromSecondsD(1);

}  // namespace

std::unique_ptr<OpenXrApiWrapper> OpenXrApiWrapper::Create() {
  std::unique_ptr<OpenXrApiWrapper> openxr =
      std::make_unique<OpenXrApiWrapper>();

  if (!openxr->Initialize()) {
    return nullptr;
  }

  return openxr;
}

OpenXrApiWrapper::OpenXrApiWrapper() = default;

OpenXrApiWrapper::~OpenXrApiWrapper() {
  Uninitialize();
}

void OpenXrApiWrapper::Reset() {
  unbounded_space_ = XR_NULL_HANDLE;
  local_space_ = XR_NULL_HANDLE;
  stage_space_ = XR_NULL_HANDLE;
  view_space_ = XR_NULL_HANDLE;
  color_swapchain_ = XR_NULL_HANDLE;
  session_ = XR_NULL_HANDLE;
  blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;
  stage_bounds_ = {};
  system_ = kInvalidSystem;
  instance_ = XR_NULL_HANDLE;

  view_configs_.clear();
  color_swapchain_images_.clear();
  frame_state_ = {};
  origin_from_eye_views_.clear();
  head_from_eye_views_.clear();
  layer_projection_views_.clear();
}

bool OpenXrApiWrapper::Initialize() {
  Reset();
  session_ended_ = false;
  pending_frame_ = false;
  // Set to min so that the first call to EnsureEventPolling is guaranteed to
  // call ProcessEvents, which will update this variable from there on.
  last_process_events_time_ = base::TimeTicks::Min();

  if (XR_FAILED(CreateInstance(&instance_, &instance_metadata_))) {
    return false;
  }
  DCHECK(HasInstance());

  if (XR_FAILED(InitializeSystem())) {
    // When initialization fails, the caller should release this object, so we
    // don't need to destroy the instance created above as it is destroyed in
    // the destructor.
    DCHECK(!IsInitialized());
    return false;
  }

  DCHECK(IsInitialized());

  if (test_hook_) {
    // Allow our mock implementation of OpenXr to be controlled by tests.
    // The mock implementation of xrCreateInstance returns a pointer to the
    // service test hook (g_test_helper) as the instance.
    service_test_hook_ = reinterpret_cast<ServiceTestHook*>(instance_);
    service_test_hook_->SetTestHook(test_hook_);

    test_hook_->AttachCurrentThread();
  }

  return true;
}

bool OpenXrApiWrapper::IsInitialized() const {
  return HasInstance() && HasSystem();
}

void OpenXrApiWrapper::Uninitialize() {
  // Destroying an instance in OpenXr also destroys all child objects of that
  // instance (including the session, swapchain, and spaces objects),
  // so they don't need to be manually destroyed.
  if (HasInstance()) {
    xrDestroyInstance(instance_);
  }

  if (test_hook_)
    test_hook_->DetachCurrentThread();

  Reset();
  session_ended_ = true;
  pending_frame_ = false;

  // Set to max so events are no longer polled in the EnsureEventPolling loop.
  last_process_events_time_ = base::TimeTicks::Max();
}

bool OpenXrApiWrapper::HasInstance() const {
  return instance_ != XR_NULL_HANDLE;
}

bool OpenXrApiWrapper::HasSystem() const {
  return system_ != kInvalidSystem && view_configs_.size() == kNumViews &&
         HasBlendMode();
}

bool OpenXrApiWrapper::HasBlendMode() const {
  return blend_mode_ != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;
}

bool OpenXrApiWrapper::HasSession() const {
  return session_ != XR_NULL_HANDLE;
}

bool OpenXrApiWrapper::HasColorSwapChain() const {
  return color_swapchain_ != XR_NULL_HANDLE &&
         color_swapchain_images_.size() > 0;
}

bool OpenXrApiWrapper::HasSpace(XrReferenceSpaceType type) const {
  switch (type) {
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      return local_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      return view_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_STAGE:
      return stage_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
      return unbounded_space_ != XR_NULL_HANDLE;
    default:
      NOTREACHED();
      return false;
  }
}

bool OpenXrApiWrapper::HasFrameState() const {
  return frame_state_.type == XR_TYPE_FRAME_STATE;
}

XrResult OpenXrApiWrapper::InitializeSystem() {
  DCHECK(HasInstance());
  DCHECK(!HasSystem());

  XrSystemId system;
  RETURN_IF_XR_FAILED(GetSystem(instance_, &system));

  uint32_t view_count;
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurationViews(
      instance_, system, kSupportedViewConfiguration, 0, &view_count, nullptr));

  // It would be an error for an OpenXr runtime to return anything other than 2
  // views to an app that only requested
  // XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO.
  DCHECK(view_count == kNumViews);

  std::vector<XrViewConfigurationView> view_configs(
      view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurationViews(
      instance_, system, kSupportedViewConfiguration, view_count, &view_count,
      view_configs.data()));

  RETURN_IF_XR_FAILED(PickEnvironmentBlendMode(system));

  // Only assign the member variables on success. If any of the above XR calls
  // fail, the vector cleans up view_configs if necessary. system does not need
  // to be cleaned up because it is not allocated.
  system_ = system;
  view_configs_ = std::move(view_configs);

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::PickEnvironmentBlendMode(XrSystemId system) {
  const std::array<XrEnvironmentBlendMode, 2> kSupportedBlendMode = {
      XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
      XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
  };
  DCHECK(HasInstance());

  uint32_t blend_mode_count;
  RETURN_IF_XR_FAILED(xrEnumerateEnvironmentBlendModes(
      instance_, system, kSupportedViewConfiguration, 0, &blend_mode_count,
      nullptr));

  std::vector<XrEnvironmentBlendMode> blend_modes(blend_mode_count);
  RETURN_IF_XR_FAILED(xrEnumerateEnvironmentBlendModes(
      instance_, system, kSupportedViewConfiguration, blend_mode_count,
      &blend_mode_count, blend_modes.data()));

  auto* blend_mode_it =
      std::find_first_of(kSupportedBlendMode.begin(), kSupportedBlendMode.end(),
                         blend_modes.begin(), blend_modes.end());
  if (blend_mode_it == kSupportedBlendMode.end()) {
    return XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED;
  }

  blend_mode_ = *blend_mode_it;
  return XR_SUCCESS;
}

bool OpenXrApiWrapper::UpdateAndGetSessionEnded() {
  // Ensure we have the latest state from the OpenXR runtime.
  if (XR_FAILED(ProcessEvents())) {
    DCHECK(session_ended_);
  }
  return session_ended_;
}

// Callers of this function must check the XrResult return value and destroy
// this OpenXrApiWrapper object on failure to clean up any intermediate
// objects that may have been created before the failure.
XrResult OpenXrApiWrapper::InitSession(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device,
    std::unique_ptr<OpenXRInputHelper>* input_helper) {
  DCHECK(d3d_device.Get());
  DCHECK(IsInitialized());

  RETURN_IF_XR_FAILED(CreateSession(d3d_device));
  RETURN_IF_XR_FAILED(CreateSwapchain());
  RETURN_IF_XR_FAILED(
      CreateSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, &local_space_));
  RETURN_IF_XR_FAILED(CreateSpace(XR_REFERENCE_SPACE_TYPE_VIEW, &view_space_));

  // It's ok if stage_space_ fails since not all OpenXR devices are required to
  // support this reference space.
  CreateSpace(XR_REFERENCE_SPACE_TYPE_STAGE, &stage_space_);
  UpdateStageBounds();

  if (instance_metadata_.unboundedReferenceSpaceSupported) {
    RETURN_IF_XR_FAILED(
        CreateSpace(XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT, &unbounded_space_));
  }

  RETURN_IF_XR_FAILED(CreateGamepadHelper(input_helper));

  // Since the objects in these arrays are used on every frame,
  // we don't want to create and destroy these objects every frame,
  // so create the number of objects we need and reuse them.
  origin_from_eye_views_.resize(kNumViews);
  head_from_eye_views_.resize(kNumViews);
  layer_projection_views_.resize(kNumViews);

  // Make sure all of the objects we initialized are there.
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_VIEW));
  DCHECK(input_helper);

  EnsureEventPolling();

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::CreateSession(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device) {
  DCHECK(d3d_device.Get());
  DCHECK(!HasSession());
  DCHECK(IsInitialized());

  XrGraphicsBindingD3D11KHR d3d11_binding = {
      XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
  d3d11_binding.device = d3d_device.Get();

  XrSessionCreateInfo session_create_info = {XR_TYPE_SESSION_CREATE_INFO};
  session_create_info.next = &d3d11_binding;
  session_create_info.systemId = system_;

  return xrCreateSession(instance_, &session_create_info, &session_);
}

XrResult OpenXrApiWrapper::CreateSwapchain() {
  DCHECK(IsInitialized());
  DCHECK(HasSession());
  DCHECK(!HasColorSwapChain());

  gfx::Size view_size = GetViewSize();

  XrSwapchainCreateInfo swapchain_create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swapchain_create_info.arraySize = 1;
  // OpenXR's swapchain format expects to describe the texture content.
  // The result of a swapchain image created from OpenXR API always contains a
  // typeless texture. On the other hand, WebGL API uses CSS color convention
  // that's sRGB. The RGBA typelss texture from OpenXR swapchain image leads to
  // a linear format render target view (reference to function
  // D3D11TextureHelper::EnsureRenderTargetView in d3d11_texture_helper.cc).
  // Therefore, the content in this openxr swapchain image is in sRGB format.
  swapchain_create_info.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  // WebVR and WebXR textures are double wide, meaning the texture contains
  // both the left and the right eye, so the width of the swapchain texture
  // needs to be doubled.
  swapchain_create_info.width = view_size.width() * 2;
  swapchain_create_info.height = view_size.height();
  swapchain_create_info.mipCount = 1;
  swapchain_create_info.faceCount = 1;
  swapchain_create_info.sampleCount = GetRecommendedSwapchainSampleCount();
  swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  XrSwapchain color_swapchain;
  RETURN_IF_XR_FAILED(
      xrCreateSwapchain(session_, &swapchain_create_info, &color_swapchain));

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));

  std::vector<XrSwapchainImageD3D11KHR> color_swapchain_images(chain_length);
  for (XrSwapchainImageD3D11KHR& image : color_swapchain_images) {
    image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
  }

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          color_swapchain_images.data())));

  color_swapchain_ = color_swapchain;
  color_swapchain_images_ = std::move(color_swapchain_images);
  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::CreateSpace(XrReferenceSpaceType type,
                                       XrSpace* space) {
  DCHECK(HasSession());
  DCHECK(!HasSpace(type));

  XrReferenceSpaceCreateInfo space_create_info = {
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  space_create_info.referenceSpaceType = type;
  space_create_info.poseInReferenceSpace = PoseIdentity();

  return xrCreateReferenceSpace(session_, &space_create_info, space);
}

XrResult OpenXrApiWrapper::CreateGamepadHelper(
    std::unique_ptr<OpenXRInputHelper>* input_helper) {
  DCHECK(HasSession());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));

  return OpenXRInputHelper::CreateOpenXRInputHelper(instance_, session_,
                                                    local_space_, input_helper);
}

XrResult OpenXrApiWrapper::BeginSession() {
  DCHECK(HasSession());

  XrSessionBeginInfo session_begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
  session_begin_info.primaryViewConfigurationType = kSupportedViewConfiguration;

  return xrBeginSession(session_, &session_begin_info);
}

XrResult OpenXrApiWrapper::BeginFrame(
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* texture) {
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());

  DCHECK(!session_ended_);

  XrFrameWaitInfo wait_frame_info = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frame_state = {XR_TYPE_FRAME_STATE};
  RETURN_IF_XR_FAILED(xrWaitFrame(session_, &wait_frame_info, &frame_state));
  frame_state_ = frame_state;

  XrFrameBeginInfo begin_frame_info = {XR_TYPE_FRAME_BEGIN_INFO};
  RETURN_IF_XR_FAILED(xrBeginFrame(session_, &begin_frame_info));
  pending_frame_ = true;

  XrSwapchainImageAcquireInfo acquire_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  uint32_t color_swapchain_image_index;
  RETURN_IF_XR_FAILED(xrAcquireSwapchainImage(color_swapchain_, &acquire_info,
                                              &color_swapchain_image_index));

  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait_info.timeout = XR_INFINITE_DURATION;

  RETURN_IF_XR_FAILED(xrWaitSwapchainImage(color_swapchain_, &wait_info));
  RETURN_IF_XR_FAILED(UpdateProjectionLayers());

  *texture = color_swapchain_images_[color_swapchain_image_index].texture;

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::EndFrame() {
  DCHECK(pending_frame_);
  DCHECK(HasBlendMode());
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasFrameState());

  XrSwapchainImageReleaseInfo release_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  RETURN_IF_XR_FAILED(xrReleaseSwapchainImage(color_swapchain_, &release_info));

  XrCompositionLayerProjection multi_projection_layer = {
      XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  XrCompositionLayerProjection* multi_projection_layer_ptr =
      &multi_projection_layer;
  multi_projection_layer.space = local_space_;
  multi_projection_layer.viewCount = origin_from_eye_views_.size();
  multi_projection_layer.views = layer_projection_views_.data();

  XrFrameEndInfo end_frame_info = {XR_TYPE_FRAME_END_INFO};
  end_frame_info.environmentBlendMode = blend_mode_;
  end_frame_info.layerCount = 1;
  end_frame_info.layers =
      reinterpret_cast<const XrCompositionLayerBaseHeader* const*>(
          &multi_projection_layer_ptr);
  end_frame_info.displayTime = frame_state_.predictedDisplayTime;

  RETURN_IF_XR_FAILED(xrEndFrame(session_, &end_frame_info));
  pending_frame_ = false;

  return XR_SUCCESS;
}

bool OpenXrApiWrapper::HasPendingFrame() const {
  return pending_frame_;
}

XrResult OpenXrApiWrapper::UpdateProjectionLayers() {
  RETURN_IF_XR_FAILED(
      LocateViews(XR_REFERENCE_SPACE_TYPE_LOCAL, &origin_from_eye_views_));
  RETURN_IF_XR_FAILED(
      LocateViews(XR_REFERENCE_SPACE_TYPE_VIEW, &head_from_eye_views_));

  gfx::Size view_size = GetViewSize();
  for (uint32_t view_index = 0; view_index < origin_from_eye_views_.size();
       view_index++) {
    const XrView& view = origin_from_eye_views_[view_index];

    XrCompositionLayerProjectionView& layer_projection_view =
        layer_projection_views_[view_index];

    layer_projection_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    layer_projection_view.pose = view.pose;
    layer_projection_view.fov = view.fov;
    layer_projection_view.subImage.swapchain = color_swapchain_;
    // Since we're in double wide mode, the texture
    // array only has one texture and is always index 0.
    layer_projection_view.subImage.imageArrayIndex = 0;
    layer_projection_view.subImage.imageRect.extent.width = view_size.width();
    layer_projection_view.subImage.imageRect.extent.height = view_size.height();
    // x coordinates is 0 for first view, 0 + i*width for ith view.
    layer_projection_view.subImage.imageRect.offset.x =
        view_size.width() * view_index;
    layer_projection_view.subImage.imageRect.offset.y = 0;
  }

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::LocateViews(XrReferenceSpaceType type,
                                       std::vector<XrView>* views) const {
  DCHECK(HasSession());

  XrViewState view_state = {XR_TYPE_VIEW_STATE};
  XrViewLocateInfo view_locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
  view_locate_info.viewConfigurationType = kSupportedViewConfiguration;
  view_locate_info.displayTime = frame_state_.predictedDisplayTime;

  switch (type) {
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      view_locate_info.space = local_space_;
      break;
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      view_locate_info.space = view_space_;
      break;
    case XR_REFERENCE_SPACE_TYPE_STAGE:
    case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
    case XR_REFERENCE_SPACE_TYPE_MAX_ENUM:
      NOTREACHED();
  }

  // Initialize the XrView objects' type field to XR_TYPE_VIEW. xrLocateViews
  // fails validation if this isn't set.
  std::vector<XrView> new_views(kNumViews, {XR_TYPE_VIEW});
  uint32_t view_count;
  RETURN_IF_XR_FAILED(xrLocateViews(session_, &view_locate_info, &view_state,
                                    new_views.size(), &view_count,
                                    new_views.data()));
  DCHECK(view_count == kNumViews);

  // If the position or orientation is not valid, don't update the views so that
  // the previous valid views are used instead.
  if ((view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
      (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
    *views = std::move(new_views);
  }

  return XR_SUCCESS;
}

// Returns the next predicted display time in nanoseconds.
XrTime OpenXrApiWrapper::GetPredictedDisplayTime() const {
  DCHECK(IsInitialized());
  DCHECK(HasFrameState());

  return frame_state_.predictedDisplayTime;
}

XrResult OpenXrApiWrapper::GetHeadPose(
    base::Optional<gfx::Quaternion>* orientation,
    base::Optional<gfx::Point3F>* position,
    bool* emulated_position) const {
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_VIEW));

  XrSpaceLocation view_from_local = {XR_TYPE_SPACE_LOCATION};
  RETURN_IF_XR_FAILED(xrLocateSpace(view_space_, local_space_,
                                    frame_state_.predictedDisplayTime,
                                    &view_from_local));

  // emulated_position indicates when there is a fallback from a fully-tracked
  // (i.e. 6DOF) type case to some form of orientation-only type tracking
  // (i.e. 3DOF/IMU type sensors)
  // Thus we have to make sure orientation is tracked.
  // Valid Bit only indicates it's either tracked or emulated, we have to check
  // for XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT to make sure orientation is
  // tracked.
  if (view_from_local.locationFlags &
      XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) {
    *orientation = gfx::Quaternion(
        view_from_local.pose.orientation.x, view_from_local.pose.orientation.y,
        view_from_local.pose.orientation.z, view_from_local.pose.orientation.w);
  } else {
    *orientation = base::nullopt;
  }

  if (view_from_local.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
    *position = gfx::Point3F(view_from_local.pose.position.x,
                             view_from_local.pose.position.y,
                             view_from_local.pose.position.z);
  } else {
    *position = base::nullopt;
  }

  *emulated_position = true;
  if (view_from_local.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {
    *emulated_position = false;
  }

  return XR_SUCCESS;
}

void OpenXrApiWrapper::GetHeadFromEyes(XrView* left, XrView* right) const {
  DCHECK(HasSession());

  *left = head_from_eye_views_[0];
  *right = head_from_eye_views_[1];
}

XrResult OpenXrApiWrapper::GetLuid(LUID* luid) const {
  DCHECK(IsInitialized());

  XrGraphicsRequirementsD3D11KHR graphics_requirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
  RETURN_IF_XR_FAILED(xrGetD3D11GraphicsRequirementsKHR(
      instance_, system_, &graphics_requirements));

  luid->LowPart = graphics_requirements.adapterLuid.LowPart;
  luid->HighPart = graphics_requirements.adapterLuid.HighPart;

  return XR_SUCCESS;
}

void OpenXrApiWrapper::EnsureEventPolling() {
  // Events are usually processed at the beginning of a frame. When frames
  // aren't being requested, this timer loop ensures OpenXR events are
  // occasionally polled while the session is active.
  if (!session_ended_) {
    if (base::TimeTicks::Now() - last_process_events_time_ >
        kTimeBetweenPollingEvents) {
      if (XR_FAILED(ProcessEvents())) {
        DCHECK(session_ended_);
      }
    }

    // Verify that the session is still active after processing events.
    if (!session_ended_) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&OpenXrApiWrapper::EnsureEventPolling,
                         weak_ptr_factory_.GetWeakPtr()),
          kTimeBetweenPollingEvents);
    }
  }
}

XrResult OpenXrApiWrapper::ProcessEvents() {
  XrEventDataBuffer event_data{XR_TYPE_EVENT_DATA_BUFFER};
  XrResult xr_result = xrPollEvent(instance_, &event_data);

  while (XR_SUCCEEDED(xr_result) && xr_result != XR_EVENT_UNAVAILABLE) {
    if (event_data.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      XrEventDataSessionStateChanged* session_state_changed =
          reinterpret_cast<XrEventDataSessionStateChanged*>(&event_data);
      // We only have will only have one session and we should make sure the
      // session that is having state_changed event is ours.
      DCHECK(session_state_changed->session == session_);
      switch (session_state_changed->state) {
        case XR_SESSION_STATE_READY:
          xr_result = BeginSession();
          break;
        case XR_SESSION_STATE_STOPPING:
          session_ended_ = true;
          xr_result = xrEndSession(session_);
          break;
        case XR_SESSION_STATE_SYNCHRONIZED:
          visibility_changed_callback_.Run(
              device::mojom::XRVisibilityState::HIDDEN);
          break;
        case XR_SESSION_STATE_VISIBLE:
          visibility_changed_callback_.Run(
              device::mojom::XRVisibilityState::VISIBLE_BLURRED);
          break;
        case XR_SESSION_STATE_FOCUSED:
          visibility_changed_callback_.Run(
              device::mojom::XRVisibilityState::VISIBLE);
          break;
        default:
          break;
      }
    } else if (event_data.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
      DCHECK(session_ != XR_NULL_HANDLE);
      Uninitialize();
      return XR_ERROR_INSTANCE_LOST;
    } else if (event_data.type ==
               XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
      XrEventDataReferenceSpaceChangePending* reference_space_change_pending =
          reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(
              &event_data);
      DCHECK(reference_space_change_pending->session == session_);
      // TODO(crbug.com/1015049)
      // Currently WMR only throw reference space change event for stage.
      // Other runtimes may decide to do it differently.
      if (reference_space_change_pending->referenceSpaceType ==
          XR_REFERENCE_SPACE_TYPE_STAGE) {
        UpdateStageBounds();
      }
    } else if (event_data.type ==
               XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
      XrEventDataInteractionProfileChanged* interaction_profile_changed =
          reinterpret_cast<XrEventDataInteractionProfileChanged*>(&event_data);
      DCHECK(interaction_profile_changed->session == session_);
      interaction_profile_changed_callback_.Run(&xr_result);
    }

    if (XR_FAILED(xr_result)) {
      Uninitialize();
      return xr_result;
    }

    event_data.type = XR_TYPE_EVENT_DATA_BUFFER;
    xr_result = xrPollEvent(instance_, &event_data);
  }

  last_process_events_time_ = base::TimeTicks::Now();

  if (XR_FAILED(xr_result))
    Uninitialize();
  return xr_result;
}

gfx::Size OpenXrApiWrapper::GetViewSize() const {
  DCHECK(IsInitialized());
  CHECK(view_configs_.size() == kNumViews);

  return gfx::Size(std::max(view_configs_[0].recommendedImageRectWidth,
                            view_configs_[1].recommendedImageRectWidth),
                   std::max(view_configs_[0].recommendedImageRectHeight,
                            view_configs_[1].recommendedImageRectHeight));
}

uint32_t OpenXrApiWrapper::GetRecommendedSwapchainSampleCount() const {
  DCHECK(IsInitialized());

  auto start = view_configs_.begin();
  auto end = view_configs_.end();

  auto compareSwapchainCounts = [](const XrViewConfigurationView& i,
                                   const XrViewConfigurationView& j) {
    return i.recommendedSwapchainSampleCount <
           j.recommendedSwapchainSampleCount;
  };

  return std::min_element(start, end, compareSwapchainCounts)
      ->recommendedSwapchainSampleCount;
}

// stage bounds is fixed unless we received event
// XrEventDataReferenceSpaceChangePending
XrResult OpenXrApiWrapper::UpdateStageBounds() {
  DCHECK(HasSession());
  XrResult xr_result = xrGetReferenceSpaceBoundsRect(
      session_, XR_REFERENCE_SPACE_TYPE_STAGE, &stage_bounds_);
  if (XR_FAILED(xr_result)) {
    stage_bounds_.height = 0;
    stage_bounds_.width = 0;
  }

  return xr_result;
}

bool OpenXrApiWrapper::GetStageParameters(XrExtent2Df* stage_bounds,
                                          gfx::Transform* local_from_stage) {
  DCHECK(stage_bounds);
  DCHECK(local_from_stage);
  DCHECK(HasSession());

  if (!HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL))
    return false;

  if (!HasSpace(XR_REFERENCE_SPACE_TYPE_STAGE))
    return false;

  *stage_bounds = stage_bounds_;

  XrSpaceLocation local_from_stage_location = {XR_TYPE_SPACE_LOCATION};
  if (FAILED(xrLocateSpace(local_space_, stage_space_,
                           frame_state_.predictedDisplayTime,
                           &local_from_stage_location)) ||
      !(local_from_stage_location.locationFlags &
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) ||
      !(local_from_stage_location.locationFlags &
        XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
    return false;
  }

  // Convert the orientation and translation given by runtime into a
  // transformation matrix.
  gfx::DecomposedTransform local_from_stage_decomp;
  local_from_stage_decomp.quaternion =
      gfx::Quaternion(local_from_stage_location.pose.orientation.x,
                      local_from_stage_location.pose.orientation.y,
                      local_from_stage_location.pose.orientation.z,
                      local_from_stage_location.pose.orientation.w);
  local_from_stage_decomp.translate[0] =
      local_from_stage_location.pose.position.x;
  local_from_stage_decomp.translate[1] =
      local_from_stage_location.pose.position.y;
  local_from_stage_decomp.translate[2] =
      local_from_stage_location.pose.position.z;

  *local_from_stage = gfx::ComposeTransform(local_from_stage_decomp);
  return true;
}

void OpenXrApiWrapper::RegisterInteractionProfileChangeCallback(
    const base::RepeatingCallback<void(XrResult*)>&
        interaction_profile_callback) {
  interaction_profile_changed_callback_ =
      std::move(interaction_profile_callback);
}

void OpenXrApiWrapper::RegisterVisibilityChangeCallback(
    const base::RepeatingCallback<void(mojom::XRVisibilityState)>&
        visibility_changed_callback) {
  visibility_changed_callback_ = std::move(visibility_changed_callback);
}

VRTestHook* OpenXrApiWrapper::test_hook_ = nullptr;
ServiceTestHook* OpenXrApiWrapper::service_test_hook_ = nullptr;
void OpenXrApiWrapper::SetTestHook(VRTestHook* hook) {
  // This may be called from any thread - tests are responsible for
  // maintaining thread safety, typically by not changing the test hook
  // while presenting.
  test_hook_ = hook;
  if (service_test_hook_) {
    service_test_hook_->SetTestHook(test_hook_);
  }
}

}  // namespace device
