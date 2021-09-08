// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_device.h"

#include <algorithm>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/numerics/math_constants.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore_gl.h"
#include "device/vr/android/arcore/arcore_gl_thread.h"
#include "device/vr/android/arcore/arcore_impl.h"
#include "device/vr/android/arcore/arcore_session_utils.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"

using base::android::JavaRef;

namespace {
constexpr float kDegreesPerRadian = 180.0f / base::kPiFloat;
}  // namespace

namespace device {

namespace {

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(const gfx::Size& frame_size) {
  mojom::XRViewPtr view = mojom::XRView::New();
  // ARCore is monoscopic and does not have an associated eye.
  view->eye = mojom::XREye::kNone;
  view->field_of_view = mojom::VRFieldOfView::New();
  // TODO(lincolnfrog): get these values for real (see gvr device).
  double fov_x = 1437.387;
  double fov_y = 1438.074;
  // TODO(lincolnfrog): get real camera intrinsics.
  int width = frame_size.width();
  int height = frame_size.height();
  float horizontal_degrees = atan(width / (2.0 * fov_x)) * kDegreesPerRadian;
  float vertical_degrees = atan(height / (2.0 * fov_y)) * kDegreesPerRadian;
  view->field_of_view->left_degrees = horizontal_degrees;
  view->field_of_view->right_degrees = horizontal_degrees;
  view->field_of_view->up_degrees = vertical_degrees;
  view->field_of_view->down_degrees = vertical_degrees;
  view->viewport = gfx::Size(width, height);

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();
  device->views.emplace_back(std::move(view));

  return device;
}

}  // namespace

ArCoreDevice::SessionState::SessionState() = default;
ArCoreDevice::SessionState::~SessionState() = default;

ArCoreDevice::ArCoreDevice(
    std::unique_ptr<ArCoreFactory> arcore_factory,
    std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
    std::unique_ptr<MailboxToSurfaceBridgeFactory>
        mailbox_to_surface_bridge_factory,
    std::unique_ptr<ArCoreSessionUtils> arcore_session_utils,
    XrFrameSinkClientFactory xr_frame_sink_client_factory)
    : VRDeviceBase(mojom::XRDeviceId::ARCORE_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      arcore_factory_(std::move(arcore_factory)),
      ar_image_transport_factory_(std::move(ar_image_transport_factory)),
      mailbox_bridge_factory_(std::move(mailbox_to_surface_bridge_factory)),
      arcore_session_utils_(std::move(arcore_session_utils)),
      xr_frame_sink_client_factory_(std::move(xr_frame_sink_client_factory)),
      mailbox_bridge_(mailbox_bridge_factory_->Create()),
      session_state_(std::make_unique<ArCoreDevice::SessionState>()) {
  // Ensure display_info_ is set to avoid crash in CallDeferredSessionCallback
  // if initialization fails. Use an arbitrary but really low resolution to make
  // it obvious if we're using this data instead of the actual values we get
  // from the output drawing surface.
  SetVRDisplayInfo(CreateVRDisplayInfo({16, 16}));

  // ARCORE always support AR blend modes
  SetArBlendModeSupported(true);
}

ArCoreDevice::~ArCoreDevice() {
  // If there's still a pending session request, reject it.
  CallDeferredRequestSessionCallback(absl::nullopt);

  // Ensure that any active sessions are terminated. Terminating the GL thread
  // would normally do so via its session_shutdown_callback_, but that happens
  // asynchronously via CreateMainThreadCallback, and it doesn't seem safe to
  // depend on all posted tasks being handled before the thread is shut down.
  // Repeated EndSession calls are a no-op, so it's OK to do this redundantly.
  OnSessionEnded();

  // The GL thread must be terminated since it uses our members. For example,
  // there might still be a posted Initialize() call in flight that uses
  // arcore_session_utils_ and arcore_factory_. Ensure that the thread is
  // stopped before other members get destructed. Don't call Stop() here,
  // destruction calls Stop() and doing so twice is illegal (null pointer
  // dereference).
  session_state_->arcore_gl_thread_ = nullptr;
}

void ArCoreDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());
  DCHECK(options->mode == device::mojom::XRSessionMode::kImmersiveAr);

  if (HasExclusiveSession()) {
    DVLOG(1) << __func__ << ": Rejecting additional session request";
    std::move(callback).Run(nullptr);
    return;
  }

  // Set HasExclusiveSession status to true. This lasts until OnSessionEnded.
  OnStartPresenting();

  DCHECK(!session_state_->pending_request_session_callback_);
  session_state_->pending_request_session_callback_ = std::move(callback);
  session_state_->required_features_.insert(options->required_features.begin(),
                                            options->required_features.end());
  session_state_->optional_features_.insert(options->optional_features.begin(),
                                            options->optional_features.end());

  const bool use_dom_overlay =
      base::Contains(options->required_features,
                     device::mojom::XRSessionFeature::DOM_OVERLAY) ||
      base::Contains(options->optional_features,
                     device::mojom::XRSessionFeature::DOM_OVERLAY);

  session_state_->depth_options_ = std::move(options->depth_options);

  // mailbox_bridge_ is either supplied from the constructor, or recreated in
  // OnSessionEnded().
  DCHECK(mailbox_bridge_);

  // We create the FrameSinkClient here and clear it in OnSessionEnded.
  DCHECK(!frame_sink_client_);
  frame_sink_client_ = xr_frame_sink_client_factory_.Run(
      options->render_process_id, options->render_frame_id);
  DCHECK(frame_sink_client_);

  for (auto& image : options->tracked_images) {
    DVLOG(3) << __func__ << ": tracked image size_in_pixels="
             << image->size_in_pixels.ToString();
    session_state_->tracked_images_.push_back(std::move(image));
  }

  session_state_->arcore_gl_thread_ = std::make_unique<ArCoreGlThread>(
      std::move(ar_image_transport_factory_), std::move(mailbox_bridge_),
      CreateMainThreadCallback(
          base::BindOnce(&ArCoreDevice::OnGlThreadReady, GetWeakPtr(),
                         options->render_process_id, options->render_frame_id,
                         use_dom_overlay)));
  session_state_->arcore_gl_thread_->Start();
}

void ArCoreDevice::OnGlThreadReady(int render_process_id,
                                   int render_frame_id,
                                   bool use_overlay) {
  auto ready_callback =
      base::BindRepeating(&ArCoreDevice::OnDrawingSurfaceReady, GetWeakPtr());
  auto touch_callback =
      base::BindRepeating(&ArCoreDevice::OnDrawingSurfaceTouch, GetWeakPtr());
  auto destroyed_callback =
      base::BindOnce(&ArCoreDevice::OnDrawingSurfaceDestroyed, GetWeakPtr());

  bool can_render_dom_content =
      session_state_->arcore_gl_thread_->GetArCoreGl()->CanRenderDOMContent();

  arcore_session_utils_->RequestArSession(
      render_process_id, render_frame_id, use_overlay, can_render_dom_content,
      std::move(ready_callback), std::move(touch_callback),
      std::move(destroyed_callback));
}

void ArCoreDevice::OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                                         gpu::SurfaceHandle surface_handle,
                                         ui::WindowAndroid* root_window,
                                         display::Display::Rotation rotation,
                                         const gfx::Size& frame_size) {
  DVLOG(1) << __func__ << ": size=" << frame_size.width() << "x"
           << frame_size.height() << " rotation=" << static_cast<int>(rotation);
  DCHECK(!session_state_->is_arcore_gl_initialized_);

  auto display_info = CreateVRDisplayInfo(frame_size);
  SetVRDisplayInfo(std::move(display_info));

  RequestArCoreGlInitialization(window, surface_handle, root_window, rotation,
                                frame_size);
}

void ArCoreDevice::OnDrawingSurfaceTouch(bool is_primary,
                                         bool touching,
                                         int32_t pointer_id,
                                         const gfx::PointF& location) {
  DVLOG(2) << __func__ << ": pointer_id=" << pointer_id
           << " is_primary=" << is_primary << " touching=" << touching;

  if (!session_state_->is_arcore_gl_initialized_ ||
      !session_state_->arcore_gl_thread_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::OnScreenTouch,
      session_state_->arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
      is_primary, touching, pointer_id, location));
}

void ArCoreDevice::OnDrawingSurfaceDestroyed() {
  DVLOG(1) << __func__;

  CallDeferredRequestSessionCallback(absl::nullopt);

  OnSessionEnded();
}

void ArCoreDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  DVLOG(2) << __func__;
  OnDrawingSurfaceDestroyed();
  std::move(on_completed).Run();
}

void ArCoreDevice::OnSessionEnded() {
  DVLOG(1) << __func__;

  if (!HasExclusiveSession())
    return;

  // This may be a no-op in case session end was initiated from the Java side.
  arcore_session_utils_->EndSession();

  // The GL thread had initialized its context with a drawing_widget based on
  // the ArImmersiveOverlay's Surface, and the one it has is no longer valid.
  // For now, just destroy the GL thread so that it is recreated for the next
  // session with fresh associated resources. Also go through these steps in
  // case the GL thread hadn't completed, or had initialized partially, to
  // ensure consistent state.

  // TODO(https://crbug.com/849568): Instead of splitting the initialization
  // of this class between construction and RequestSession, perform all the
  // initialization at once on the first successful RequestSession call.

  // If we have a frame sink client, notify it that it's surface has been
  // destroyed. While this is required in the case of the surface actually being
  // destroyed, it's a good idea to do it before we actually end the session.
  // Note that this may trigger the bindings on the session to disconnect.
  if (frame_sink_client_)
    frame_sink_client_->SurfaceDestroyed();

  // Reset per-session members to initial values.
  session_state_ = std::make_unique<ArCoreDevice::SessionState>();

  // The frame sink client is re-requested when we start a new session, but once
  // a session has ended it should be destroyed. However, it needs to outlive
  // the gl thread.
  frame_sink_client_.reset();

  // The image transport factory should be reusable, but we've std::moved it
  // to the GL thread. Make a new one for next time. (This is cheap, it's
  // just a factory.)
  ar_image_transport_factory_ = std::make_unique<ArImageTransportFactory>();

  // Create a new mailbox bridge for use in the next session. (This is cheap,
  // the constructor doesn't establish a GL context.)
  mailbox_bridge_ = mailbox_bridge_factory_->Create();

  // This sets HasExclusiveSession status to false.
  OnExitPresent();
}

void ArCoreDevice::CallDeferredRequestSessionCallback(
    absl::optional<ArCoreGlInitializeResult> initialize_result) {
  DVLOG(1) << __func__ << " success=" << initialize_result.has_value();
  DCHECK(IsOnMainThread());

  // We might not have any pending session requests, i.e. if destroyed
  // immediately after construction.
  if (!session_state_->pending_request_session_callback_)
    return;

  mojom::XRRuntime::RequestSessionCallback deferred_callback =
      std::move(session_state_->pending_request_session_callback_);

  if (!initialize_result) {
    std::move(deferred_callback).Run(nullptr);
    return;
  }

  // Success case should only happen after GL thread is ready.
  auto create_callback = base::BindOnce(
      &ArCoreDevice::OnCreateSessionCallback, GetWeakPtr(),
      std::move(deferred_callback), std::move(*initialize_result));

  auto shutdown_callback =
      base::BindOnce(&ArCoreDevice::OnSessionEnded, GetWeakPtr());

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::CreateSession,
      session_state_->arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
      display_info_->Clone(),
      CreateMainThreadCallback(std::move(create_callback)),
      CreateMainThreadCallback(std::move(shutdown_callback))));
}

void ArCoreDevice::OnCreateSessionCallback(
    mojom::XRRuntime::RequestSessionCallback deferred_callback,
    ArCoreGlInitializeResult initialize_result,
    ArCoreGlCreateSessionResult create_session_result) {
  DVLOG(2) << __func__;
  DCHECK(IsOnMainThread());

  auto session_result = mojom::XRRuntimeSessionResult::New();
  session_result->controller =
      std::move(create_session_result.session_controller);

  if (initialize_result.frame_sink_id.is_valid()) {
    session_result->frame_sink_id = initialize_result.frame_sink_id;
  }

  session_result->session = mojom::XRSession::New();
  auto* session = session_result->session.get();

  session->data_provider = std::move(create_session_result.frame_data_provider);
  session->display_info = std::move(create_session_result.display_info);
  session->submit_frame_sink =
      std::move(create_session_result.presentation_connection);
  session->enabled_features.assign(initialize_result.enabled_features.begin(),
                                   initialize_result.enabled_features.end());
  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  auto* config = session->device_config.get();

  config->supports_viewport_scaling = true;
  config->depth_configuration =
      initialize_result.depth_configuration
          ? initialize_result.depth_configuration->Clone()
          : nullptr;

  // ARCORE only supports immersive-ar sessions
  session->enviroment_blend_mode =
      device::mojom::XREnvironmentBlendMode::kAlphaBlend;
  session->interaction_mode = device::mojom::XRInteractionMode::kScreenSpace;

  std::move(deferred_callback).Run(std::move(session_result));
}

void ArCoreDevice::PostTaskToGlThread(base::OnceClosure task) {
  DCHECK(IsOnMainThread());
  session_state_->arcore_gl_thread_->GetArCoreGl()
      ->GetGlThreadTaskRunner()
      ->PostTask(FROM_HERE, std::move(task));
}

bool ArCoreDevice::IsOnMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

void ArCoreDevice::RequestArCoreGlInitialization(
    gfx::AcceleratedWidget drawing_widget,
    gpu::SurfaceHandle surface_handle,
    ui::WindowAndroid* root_window,
    int drawing_rotation,
    const gfx::Size& frame_size) {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());

  if (!arcore_session_utils_->EnsureLoaded()) {
    DLOG(ERROR) << "ARCore was not loaded properly.";
    OnArCoreGlInitializationComplete(absl::nullopt);
    return;
  }

  if (!session_state_->is_arcore_gl_initialized_) {
    // We will only try to initialize ArCoreGl once, at the end of the
    // permission sequence, and will resolve pending requests that have queued
    // up once that initialization completes. We set is_arcore_gl_initialized_
    // in the callback to block operations that require it to be ready.
    auto rotation = static_cast<display::Display::Rotation>(drawing_rotation);
    PostTaskToGlThread(base::BindOnce(
        &ArCoreGl::Initialize,
        session_state_->arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
        arcore_session_utils_.get(), arcore_factory_.get(),
        frame_sink_client_.get(), drawing_widget, surface_handle, root_window,
        frame_size, rotation, session_state_->required_features_,
        session_state_->optional_features_,
        std::move(session_state_->tracked_images_),
        std::move(session_state_->depth_options_),
        CreateMainThreadCallback(base::BindOnce(
            &ArCoreDevice::OnArCoreGlInitializationComplete, GetWeakPtr()))));
    return;
  }

  // Since the GL is already initialized, we already have session_state_ that we
  // can pass along.
  OnArCoreGlInitializationComplete(ArCoreGlInitializeResult(
      session_state_->enabled_features_, session_state_->depth_configuration_,
      session_state_->frame_sink_id_));
}

void ArCoreDevice::OnArCoreGlInitializationComplete(
    absl::optional<ArCoreGlInitializeResult> arcore_initialization_result) {
  DVLOG(1) << __func__ << ": arcore_initialization_result.has_value()="
           << arcore_initialization_result.has_value();
  DCHECK(IsOnMainThread());

  session_state_->is_arcore_gl_initialized_ =
      arcore_initialization_result.has_value();

  if (arcore_initialization_result) {
    session_state_->enabled_features_ =
        arcore_initialization_result->enabled_features;
    session_state_->depth_configuration_ =
        arcore_initialization_result->depth_configuration;
    session_state_->frame_sink_id_ =
        arcore_initialization_result->frame_sink_id;
  } else {
    session_state_->enabled_features_ = {};
    session_state_->depth_configuration_ = absl::nullopt;
  }

  // We only start GL initialization after the user has granted consent, so we
  // can now start the session.
  CallDeferredRequestSessionCallback(std::move(arcore_initialization_result));
}

}  // namespace device
