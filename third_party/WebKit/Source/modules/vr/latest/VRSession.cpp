// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRSession.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/v8_vr_frame_request_callback.h"
#include "core/dom/DOMException.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/vr/latest/VR.h"
#include "modules/vr/latest/VRDevice.h"
#include "modules/vr/latest/VRFrameOfReference.h"
#include "modules/vr/latest/VRFrameOfReferenceOptions.h"
#include "modules/vr/latest/VRFrameProvider.h"
#include "modules/vr/latest/VRLayer.h"
#include "modules/vr/latest/VRPresentationFrame.h"
#include "modules/vr/latest/VRSessionEvent.h"
#include "modules/vr/latest/VRView.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

namespace {

const char kSessionEnded[] = "VRSession has already ended.";

const char kUnknownFrameOfReference[] = "Unknown frame of reference type";

const char kNonEmulatedStageNotSupported[] =
    "Non-emulated 'stage' frame of reference not yet supported";

const double kDegToRad = M_PI / 180.0;

void UpdateViewFromEyeParameters(
    VRView* view,
    const device::mojom::blink::VREyeParametersPtr& eye,
    double depth_near,
    double depth_far) {
  const device::mojom::blink::VRFieldOfViewPtr& fov = eye->fieldOfView;

  view->UpdateProjectionMatrixFromFoV(
      fov->upDegrees * kDegToRad, fov->downDegrees * kDegToRad,
      fov->leftDegrees * kDegToRad, fov->rightDegrees * kDegToRad, depth_near,
      depth_far);

  view->UpdateOffset(eye->offset[0], eye->offset[1], eye->offset[2]);
}

}  // namespace

VRSession::VRSession(VRDevice* device, bool exclusive)
    : device_(device),
      exclusive_(exclusive),
      callback_collection_(device->GetExecutionContext()) {}

void VRSession::setDepthNear(double value) {
  if (depth_near_ != value) {
    views_dirty_ = true;
    depth_near_ = value;
  }
}

void VRSession::setDepthFar(double value) {
  if (depth_far_ != value) {
    views_dirty_ = true;
    depth_far_ = value;
  }
}

void VRSession::setBaseLayer(VRLayer* value) {
  base_layer_ = value;
}

ExecutionContext* VRSession::GetExecutionContext() const {
  return device_->GetExecutionContext();
}

const AtomicString& VRSession::InterfaceName() const {
  return EventTargetNames::VRSession;
}

ScriptPromise VRSession::requestFrameOfReference(
    ScriptState* script_state,
    const String& type,
    const VRFrameOfReferenceOptions& options) {
  if (detached_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, kSessionEnded));
  }

  VRFrameOfReference* frameOfRef = nullptr;
  if (type == "headModel") {
    frameOfRef =
        new VRFrameOfReference(this, VRFrameOfReference::kTypeHeadModel);
  } else if (type == "eyeLevel") {
    frameOfRef =
        new VRFrameOfReference(this, VRFrameOfReference::kTypeEyeLevel);
  } else if (type == "stage") {
    if (!options.disableStageEmulation()) {
      frameOfRef = new VRFrameOfReference(this, VRFrameOfReference::kTypeStage);
      frameOfRef->UseEmulatedHeight(options.stageEmulationHeight());
    } else {
      // TODO(bajones): Support native stages using the standing transform.
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(kNotSupportedError,
                                             kNonEmulatedStageNotSupported));
    }
  }

  if (!frameOfRef) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotSupportedError, kUnknownFrameOfReference));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(frameOfRef);

  return promise;
}

int VRSession::requestFrame(V8VRFrameRequestCallback* callback) {
  // Don't allow any new frame requests once the session is ended.
  if (detached_)
    return 0;

  // Don't allow frames to be scheduled if there's no layers attached to the
  // session. That would allow tracking with no associated visuals.
  if (!base_layer_)
    return 0;

  int id = callback_collection_.RegisterCallback(callback);
  if (!pending_frame_) {
    // Kick off a request for a new VR frame.
    device_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
  return id;
}

void VRSession::cancelFrame(int id) {
  callback_collection_.CancelCallback(id);
}

ScriptPromise VRSession::end(ScriptState* script_state) {
  // Don't allow a session to end twice.
  if (detached_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, kSessionEnded));
  }

  ForceEnd();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(bajones): If there's any work that needs to be done asynchronously on
  // session end it should be completed before this promise is resolved.

  resolver->Resolve();
  return promise;
}

void VRSession::ForceEnd() {
  // Detach this session from the device.
  detached_ = true;

  // If this session is the active exclusive session for the device, notify the
  // frameProvider that it's ended.
  if (device_->frameProvider()->exclusive_session() == this) {
    device_->frameProvider()->OnExclusiveSessionEnded();
  }

  DispatchEvent(VRSessionEvent::Create(EventTypeNames::end, this));
}

DoubleSize VRSession::IdealFramebufferSize() const {
  double width = device_->vrDisplayInfoPtr()->leftEye->renderWidth +
                 device_->vrDisplayInfoPtr()->rightEye->renderWidth;
  double height = std::max(device_->vrDisplayInfoPtr()->leftEye->renderHeight,
                           device_->vrDisplayInfoPtr()->rightEye->renderHeight);
  return DoubleSize(width, height);
}

void VRSession::OnFocus() {
  if (!blurred_)
    return;

  blurred_ = false;
  DispatchEvent(VRSessionEvent::Create(EventTypeNames::focus, this));
}

void VRSession::OnBlur() {
  if (blurred_)
    return;

  blurred_ = true;
  DispatchEvent(VRSessionEvent::Create(EventTypeNames::blur, this));
}

void VRSession::OnFrame(
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  DVLOG(2) << __FUNCTION__;
  // Don't process any outstanding frames once the session is ended.
  if (detached_)
    return;

  // Don't allow frames to be processed if there's no layers attached to the
  // session. That would allow tracking with no associated visuals.
  if (!base_layer_)
    return;

  VRPresentationFrame* presentation_frame = new VRPresentationFrame(this);
  presentation_frame->UpdateBasePose(std::move(base_pose_matrix));

  if (pending_frame_) {
    pending_frame_ = false;

    // Cache the base layer, since it could change during the frame callback.
    VRLayer* frame_base_layer = base_layer_;
    frame_base_layer->OnFrameStart();

    // Resolve the queued requestFrame callbacks. All VR rendering will happen
    // within these calls. resolving_frame_ will be true for the duration of the
    // callbacks.
    AutoReset<bool> resolving(&resolving_frame_, true);
    callback_collection_.ExecuteCallbacks(this, presentation_frame);

    frame_base_layer->OnFrameEnd();
  }
}

const HeapVector<Member<VRView>>& VRSession::views() {
  if (views_dirty_) {
    if (exclusive_) {
      // If we don't already have the views allocated, do so now.
      if (views_.IsEmpty()) {
        views_.push_back(new VRView(this, VRView::kEyeLeft));
        views_.push_back(new VRView(this, VRView::kEyeRight));
      }

      // In exclusive mode the projection and view matrices must be aligned with
      // the device's physical optics.
      UpdateViewFromEyeParameters(views_[VRView::kEyeLeft],
                                  device_->vrDisplayInfoPtr()->leftEye,
                                  depth_near_, depth_far_);
      UpdateViewFromEyeParameters(views_[VRView::kEyeRight],
                                  device_->vrDisplayInfoPtr()->rightEye,
                                  depth_near_, depth_far_);
    }

    views_dirty_ = false;
  }

  return views_;
}

void VRSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(base_layer_);
  visitor->Trace(views_);
  visitor->Trace(callback_collection_);
  EventTargetWithInlineData::Trace(visitor);
}

void VRSession::TraceWrappers(
    const blink::ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(callback_collection_);
}

}  // namespace blink
