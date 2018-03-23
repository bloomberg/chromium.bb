// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRFrameProvider.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "build/build_config.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallbackCollection.h"
#include "core/frame/LocalFrame.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "modules/xr/XR.h"
#include "modules/xr/XRDevice.h"
#include "modules/xr/XRPresentationContext.h"
#include "modules/xr/XRSession.h"
#include "modules/xr/XRViewport.h"
#include "modules/xr/XRWebGLLayer.h"
#include "platform/graphics/gpu/XRFrameTransport.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

class XRFrameProviderRequestCallback
    : public FrameRequestCallbackCollection::FrameCallback {
 public:
  explicit XRFrameProviderRequestCallback(XRFrameProvider* frame_provider)
      : frame_provider_(frame_provider) {}
  ~XRFrameProviderRequestCallback() override = default;
  void Invoke(double high_res_time_ms) override {
    frame_provider_->OnNonExclusiveVSync(high_res_time_ms / 1000.0);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(frame_provider_);

    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

  Member<XRFrameProvider> frame_provider_;
};

std::unique_ptr<TransformationMatrix> getPoseMatrix(
    const device::mojom::blink::VRPosePtr& pose) {
  if (!pose)
    return nullptr;

  std::unique_ptr<TransformationMatrix> pose_matrix =
      TransformationMatrix::Create();

  TransformationMatrix::DecomposedType decomp;

  memset(&decomp, 0, sizeof(decomp));
  decomp.perspective_w = 1;
  decomp.scale_x = 1;
  decomp.scale_y = 1;
  decomp.scale_z = 1;

  if (pose->orientation) {
    decomp.quaternion_x = -pose->orientation.value()[0];
    decomp.quaternion_y = -pose->orientation.value()[1];
    decomp.quaternion_z = -pose->orientation.value()[2];
    decomp.quaternion_w = pose->orientation.value()[3];
  } else {
    decomp.quaternion_w = 1.0;
  }

  if (pose->position) {
    decomp.translate_x = pose->position.value()[0];
    decomp.translate_y = pose->position.value()[1];
    decomp.translate_z = pose->position.value()[2];
  }

  pose_matrix->Recompose(decomp);

  return pose_matrix;
}

}  // namespace

XRFrameProvider::XRFrameProvider(XRDevice* device) : device_(device) {}

void XRFrameProvider::BeginExclusiveSession(XRSession* session,
                                            ScriptPromiseResolver* resolver) {
  // Make sure the session is indeed an exclusive one.
  DCHECK(session && session->exclusive());

  // Ensure we can only have one exclusive session at a time.
  DCHECK(!exclusive_session_);

  exclusive_session_ = session;

  pending_exclusive_session_resolver_ = resolver;

  // Establish the connection with the VSyncProvider if needed.
  if (!presentation_provider_.is_bound()) {
    frame_transport_ = new XRFrameTransport();

    // Set up RequestPresentOptions based on canvas properties.
    device::mojom::blink::VRRequestPresentOptionsPtr options =
        device::mojom::blink::VRRequestPresentOptions::New();
    options->preserve_drawing_buffer = false;
    options->webxr_input = true;

    device_->xrDisplayHostPtr()->RequestPresent(
        frame_transport_->GetSubmitFrameClient(),
        mojo::MakeRequest(&presentation_provider_), std::move(options),
        WTF::Bind(&XRFrameProvider::OnPresentComplete, WrapPersistent(this)));

    presentation_provider_.set_connection_error_handler(
        WTF::Bind(&XRFrameProvider::OnPresentationProviderConnectionError,
                  WrapWeakPersistent(this)));
  }
}

void XRFrameProvider::OnPresentComplete(
    bool success,
    device::mojom::blink::VRDisplayFrameTransportOptionsPtr transport_options) {
  if (success) {
    frame_transport_->SetTransportOptions(std::move(transport_options));
    frame_transport_->PresentChange();
    pending_exclusive_session_resolver_->Resolve(exclusive_session_);
  } else {
    exclusive_session_->ForceEnd();

    if (pending_exclusive_session_resolver_) {
      DOMException* exception = DOMException::Create(
          kNotAllowedError, "Request for exclusive XRSession was denied.");
      pending_exclusive_session_resolver_->Reject(exception);
    }
  }

  pending_exclusive_session_resolver_ = nullptr;
}

void XRFrameProvider::OnPresentationProviderConnectionError() {
  if (pending_exclusive_session_resolver_) {
    DOMException* exception = DOMException::Create(
        kNotAllowedError,
        "Error occured while requesting exclusive XRSession.");
    pending_exclusive_session_resolver_->Reject(exception);
    pending_exclusive_session_resolver_ = nullptr;
  }

  presentation_provider_.reset();
  if (vsync_connection_failed_)
    return;
  exclusive_session_->ForceEnd();
  vsync_connection_failed_ = true;
}

// Called by the exclusive session when it is ended.
void XRFrameProvider::OnExclusiveSessionEnded() {
  if (!exclusive_session_)
    return;

  device_->xrDisplayHostPtr()->ExitPresent();

  exclusive_session_ = nullptr;
  pending_exclusive_vsync_ = false;
  frame_id_ = -1;

  if (presentation_provider_.is_bound()) {
    presentation_provider_.reset();
  }

  frame_transport_ = nullptr;

  // When we no longer have an active exclusive session schedule all the
  // outstanding frames that were requested while the exclusive session was
  // active.
  if (requesting_sessions_.size() > 0)
    ScheduleNonExclusiveFrame();
}

// Schedule a session to be notified when the next XR frame is available.
void XRFrameProvider::RequestFrame(XRSession* session) {
  DVLOG(2) << __FUNCTION__;

  // If a previous session has already requested a frame don't fire off another
  // request. All requests will be fullfilled at once when OnVSync is called.
  if (session->exclusive()) {
    ScheduleExclusiveFrame();
  } else {
    requesting_sessions_.push_back(session);

    // If there's an active exclusive session save the request but suppress
    // processing it until the exclusive session is no longer active.
    if (exclusive_session_)
      return;

    ScheduleNonExclusiveFrame();
  }
}

void XRFrameProvider::ScheduleExclusiveFrame() {
  if (pending_exclusive_vsync_)
    return;

  pending_exclusive_vsync_ = true;

  presentation_provider_->GetVSync(
      WTF::Bind(&XRFrameProvider::OnExclusiveVSync, WrapWeakPersistent(this)));
}

void XRFrameProvider::ScheduleNonExclusiveFrame() {
  if (pending_non_exclusive_vsync_)
    return;

  LocalFrame* frame = device_->xr()->GetFrame();
  if (!frame)
    return;

  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  pending_non_exclusive_vsync_ = true;

  device_->xrMagicWindowProviderPtr()->GetPose(WTF::Bind(
      &XRFrameProvider::OnNonExclusivePose, WrapWeakPersistent(this)));
  doc->RequestAnimationFrame(new XRFrameProviderRequestCallback(this));
}

void XRFrameProvider::OnExclusiveVSync(
    device::mojom::blink::VRPosePtr pose,
    WTF::TimeDelta time_delta,
    int16_t frame_id,
    device::mojom::blink::VRPresentationProvider::VSyncStatus status) {
  DVLOG(2) << __FUNCTION__;
  vsync_connection_failed_ = false;
  switch (status) {
    case device::mojom::blink::VRPresentationProvider::VSyncStatus::SUCCESS:
      break;
    case device::mojom::blink::VRPresentationProvider::VSyncStatus::CLOSING:
      return;
  }

  // We may have lost the exclusive session since the last VSync request.
  if (!exclusive_session_) {
    return;
  }

  frame_pose_ = std::move(pose);
  frame_id_ = frame_id;
  pending_exclusive_vsync_ = false;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444.
  Platform::Current()->CurrentThread()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                           WrapWeakPersistent(this), time_delta.InSecondsF()));
}

void XRFrameProvider::OnNonExclusiveVSync(double timestamp) {
  DVLOG(2) << __FUNCTION__;

  pending_non_exclusive_vsync_ = false;

  // Suppress non-exclusive vsyncs when there's an exclusive session active.
  if (exclusive_session_)
    return;

  Platform::Current()->CurrentThread()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                           WrapWeakPersistent(this), timestamp));
}

void XRFrameProvider::OnNonExclusivePose(device::mojom::blink::VRPosePtr pose) {
  frame_pose_ = std::move(pose);
}

void XRFrameProvider::ProcessScheduledFrame(double timestamp) {
  DVLOG(2) << __FUNCTION__;

  TRACE_EVENT1("gpu", "XRFrameProvider::ProcessScheduledFrame", "frame",
               frame_id_);

  if (exclusive_session_) {
    if (frame_pose_ && frame_pose_->input_state.has_value()) {
      exclusive_session_->OnInputStateChange(frame_id_,
                                             frame_pose_->input_state.value());
    }

    // If there's an exclusive session active only process it's frame.
    std::unique_ptr<TransformationMatrix> pose_matrix =
        getPoseMatrix(frame_pose_);
    exclusive_session_->OnFrame(std::move(pose_matrix));
  } else {
    // In the process of fulfilling the frame requests for each session they are
    // extremely likely to request another frame. Work off of a separate list
    // from the requests to prevent infinite loops.
    HeapVector<Member<XRSession>> processing_sessions;
    swap(requesting_sessions_, processing_sessions);

    // Inform sessions with a pending request of the new frame
    for (unsigned i = 0; i < processing_sessions.size(); ++i) {
      XRSession* session = processing_sessions.at(i).Get();

      if (frame_pose_ && frame_pose_->input_state.has_value()) {
        session->OnInputStateChange(frame_id_,
                                    frame_pose_->input_state.value());
      }

      std::unique_ptr<TransformationMatrix> pose_matrix =
          getPoseMatrix(frame_pose_);
      session->OnFrame(std::move(pose_matrix));
    }
  }
}

void XRFrameProvider::SubmitWebGLLayer(XRWebGLLayer* layer) {
  DCHECK(layer);
  DCHECK(layer->session() == exclusive_session_);
  DCHECK(presentation_provider_);

  TRACE_EVENT1("gpu", "XRFrameProvider::SubmitWebGLLayer", "frame", frame_id_);

  WebGLRenderingContextBase* webgl_context = layer->context();

  frame_transport_->FramePreImage(webgl_context->ContextGL());

  std::unique_ptr<viz::SingleReleaseCallback> image_release_callback;
  scoped_refptr<StaticBitmapImage> image_ref =
      layer->TransferToStaticBitmapImage(&image_release_callback);

  if (!image_ref)
    return;

  // Hardware-accelerated rendering should always be texture backed. Ensure this
  // is the case, don't attempt to render if using an unexpected drawing path.
  if (!image_ref->IsTextureBacked()) {
    NOTREACHED() << "WebXR requires hardware-accelerated rendering to texture";
    return;
  }

  // TODO(bajones): Remove this when the Windows path has been updated to no
  // longer require a texture copy.
  bool needs_copy = device_->external();

  frame_transport_->FrameSubmit(
      presentation_provider_.get(), webgl_context->ContextGL(), webgl_context,
      std::move(image_ref), std::move(image_release_callback), frame_id_,
      needs_copy);

  // Reset our frame id, since anything we'd want to do (resizing/etc) can
  // no-longer happen to this frame.
  frame_id_ = -1;
}

// TODO(bajones): This only works because we're restricted to a single layer at
// the moment. Will need an overhaul when we get more robust layering support.
void XRFrameProvider::UpdateWebGLLayerViewports(XRWebGLLayer* layer) {
  DCHECK(layer->session() == exclusive_session_);
  DCHECK(presentation_provider_);

  XRViewport* left = layer->GetViewportForEye(XRView::kEyeLeft);
  XRViewport* right = layer->GetViewportForEye(XRView::kEyeRight);
  float width = layer->framebufferWidth();
  float height = layer->framebufferHeight();

  WebFloatRect left_coords(
      static_cast<float>(left->x()) / width,
      static_cast<float>(height - (left->y() + left->height())) / height,
      static_cast<float>(left->width()) / width,
      static_cast<float>(left->height()) / height);
  WebFloatRect right_coords(
      static_cast<float>(right->x()) / width,
      static_cast<float>(height - (right->y() + right->height())) / height,
      static_cast<float>(right->width()) / width,
      static_cast<float>(right->height()) / height);

  presentation_provider_->UpdateLayerBounds(
      frame_id_, left_coords, right_coords, WebSize(width, height));
}

void XRFrameProvider::Dispose() {
  presentation_provider_.reset();
  // TODO(bajones): Do something for outstanding frame requests?
}

void XRFrameProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(pending_exclusive_session_resolver_);
  visitor->Trace(frame_transport_);
  visitor->Trace(exclusive_session_);
  visitor->Trace(requesting_sessions_);
}

}  // namespace blink
