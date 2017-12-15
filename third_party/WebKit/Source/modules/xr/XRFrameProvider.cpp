// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRFrameProvider.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallbackCollection.h"
#include "core/frame/LocalFrame.h"
#include "modules/xr/XR.h"
#include "modules/xr/XRDevice.h"
#include "modules/xr/XRSession.h"
#include "platform/WebTaskRunner.h"
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
  ~XRFrameProviderRequestCallback() override {}
  void Invoke(double high_res_time_ms) override {
    // TODO(bajones): Eventually exclusive vsyncs won't be handled here.
    if (frame_provider_->exclusive_session()) {
      frame_provider_->OnExclusiveVSync(high_res_time_ms / 1000.0);
    } else {
      frame_provider_->OnNonExclusiveVSync(high_res_time_ms / 1000.0);
    }
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

  // TODO(bajones): Request a XRPresentationProviderPtr to use for presenting
  // frames, delay call to OnPresentComplete till the connection is established.
  OnPresentComplete(true);
}

void XRFrameProvider::OnPresentComplete(bool success) {
  if (success) {
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

// Called by the exclusive session when it is ended.
void XRFrameProvider::OnExclusiveSessionEnded() {
  if (!exclusive_session_)
    return;

  // TODO(bajones): Call device_->xrDisplayHostPtr()->ExitPresent();

  exclusive_session_ = nullptr;
  pending_exclusive_vsync_ = false;
  frame_id_ = -1;

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

  // TODO(bajones): This should acquire frames through a XRPresentationProvider
  // instead of duplicating the non-exclusive path.
  LocalFrame* frame = device_->xr()->GetFrame();
  if (!frame)
    return;

  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  pending_exclusive_vsync_ = true;

  device_->xrMagicWindowProviderPtr()->GetPose(WTF::Bind(
      &XRFrameProvider::OnNonExclusivePose, WrapWeakPersistent(this)));
  doc->RequestAnimationFrame(new XRFrameProviderRequestCallback(this));
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

void XRFrameProvider::OnExclusiveVSync(double timestamp) {
  DVLOG(2) << __FUNCTION__;

  pending_exclusive_vsync_ = false;

  // We may have lost the exclusive session since the last VSync request.
  if (!exclusive_session_)
    return;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444.
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                                 WrapWeakPersistent(this), timestamp));
}

void XRFrameProvider::OnNonExclusiveVSync(double timestamp) {
  DVLOG(2) << __FUNCTION__;

  pending_non_exclusive_vsync_ = false;

  // Suppress non-exclusive vsyncs when there's an exclusive session active.
  if (exclusive_session_)
    return;

  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
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
      std::unique_ptr<TransformationMatrix> pose_matrix =
          getPoseMatrix(frame_pose_);
      session->OnFrame(std::move(pose_matrix));
    }
  }
}

void XRFrameProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(pending_exclusive_session_resolver_);
  visitor->Trace(exclusive_session_);
  visitor->Trace(requesting_sessions_);
}

}  // namespace blink
