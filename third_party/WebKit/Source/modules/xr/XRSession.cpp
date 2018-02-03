// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRSession.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "core/dom/DOMException.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrame.h"
#include "core/resize_observer/ResizeObserver.h"
#include "core/resize_observer/ResizeObserverEntry.h"
#include "modules/EventTargetModules.h"
#include "modules/xr/XR.h"
#include "modules/xr/XRDevice.h"
#include "modules/xr/XRFrameOfReference.h"
#include "modules/xr/XRFrameOfReferenceOptions.h"
#include "modules/xr/XRFrameProvider.h"
#include "modules/xr/XRLayer.h"
#include "modules/xr/XRPresentationContext.h"
#include "modules/xr/XRPresentationFrame.h"
#include "modules/xr/XRSessionEvent.h"
#include "modules/xr/XRView.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

namespace {

const char kSessionEnded[] = "XRSession has already ended.";

const char kUnknownFrameOfReference[] = "Unknown frame of reference type";

const char kNonEmulatedStageNotSupported[] =
    "Non-emulated 'stage' frame of reference not yet supported";

const double kDegToRad = M_PI / 180.0;

// TODO(bajones): This is something that we probably want to make configurable.
const double kMagicWindowVerticalFieldOfView = 75.0f * M_PI / 180.0f;

void UpdateViewFromEyeParameters(
    XRView* view,
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

class XRSession::XRSessionResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit XRSessionResizeObserverDelegate(XRSession* session)
      : session_(session) {
    DCHECK(session);
  }
  ~XRSessionResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    session_->UpdateCanvasDimensions(entries[0]->target());
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(session_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<XRSession> session_;
};

XRSession::XRSession(XRDevice* device,
                     bool exclusive,
                     XRPresentationContext* output_context)
    : device_(device),
      exclusive_(exclusive),
      output_context_(output_context),
      callback_collection_(device->GetExecutionContext()) {
  // When an output context is provided, monitor it for resize events.
  if (output_context_) {
    HTMLCanvasElement* canvas = outputContext()->canvas();
    if (canvas) {
      resize_observer_ = ResizeObserver::Create(
          canvas->GetDocument(), new XRSessionResizeObserverDelegate(this));
      resize_observer_->observe(canvas);

      // Get the initial canvas dimensions
      UpdateCanvasDimensions(canvas);
    }
  }
}

void XRSession::setDepthNear(double value) {
  if (depth_near_ != value) {
    views_dirty_ = true;
    depth_near_ = value;
  }
}

void XRSession::setDepthFar(double value) {
  if (depth_far_ != value) {
    views_dirty_ = true;
    depth_far_ = value;
  }
}

void XRSession::setBaseLayer(XRLayer* value) {
  base_layer_ = value;
  // Make sure that the layer's drawing buffer is updated to the right size
  // if this is a non-exclusive session.
  if (!exclusive_ && base_layer_) {
    base_layer_->OnResize();
  }
}

ExecutionContext* XRSession::GetExecutionContext() const {
  return device_->GetExecutionContext();
}

const AtomicString& XRSession::InterfaceName() const {
  return EventTargetNames::XRSession;
}

ScriptPromise XRSession::requestFrameOfReference(
    ScriptState* script_state,
    const String& type,
    const XRFrameOfReferenceOptions& options) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, kSessionEnded));
  }

  XRFrameOfReference* frameOfRef = nullptr;
  if (type == "headModel") {
    frameOfRef =
        new XRFrameOfReference(this, XRFrameOfReference::kTypeHeadModel);
  } else if (type == "eyeLevel") {
    frameOfRef =
        new XRFrameOfReference(this, XRFrameOfReference::kTypeEyeLevel);
  } else if (type == "stage") {
    if (!options.disableStageEmulation()) {
      frameOfRef = new XRFrameOfReference(this, XRFrameOfReference::kTypeStage);
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

int XRSession::requestAnimationFrame(V8XRFrameRequestCallback* callback) {
  // Don't allow any new frame requests once the session is ended.
  if (ended_)
    return 0;

  // Don't allow frames to be scheduled if there's no layers attached to the
  // session. That would allow tracking with no associated visuals.
  if (!base_layer_)
    return 0;

  int id = callback_collection_.RegisterCallback(callback);
  if (!pending_frame_) {
    // Kick off a request for a new XR frame.
    device_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
  return id;
}

void XRSession::cancelAnimationFrame(int id) {
  callback_collection_.CancelCallback(id);
}

ScriptPromise XRSession::end(ScriptState* script_state) {
  // Don't allow a session to end twice.
  if (ended_) {
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

void XRSession::ForceEnd() {
  // Detach this session from the device.
  ended_ = true;
  pending_frame_ = false;

  // If this session is the active exclusive session for the device, notify the
  // frameProvider that it's ended.
  if (device_->frameProvider()->exclusive_session() == this) {
    device_->frameProvider()->OnExclusiveSessionEnded();
  }

  DispatchEvent(XRSessionEvent::Create(EventTypeNames::end, this));
}

DoubleSize XRSession::IdealFramebufferSize() const {
  if (!exclusive_) {
    return DoubleSize(output_width_, output_height_);
  }

  double width = device_->xrDisplayInfoPtr()->leftEye->renderWidth +
                 device_->xrDisplayInfoPtr()->rightEye->renderWidth;
  double height = std::max(device_->xrDisplayInfoPtr()->leftEye->renderHeight,
                           device_->xrDisplayInfoPtr()->rightEye->renderHeight);
  return DoubleSize(width, height);
}

void XRSession::OnFocus() {
  if (!blurred_)
    return;

  blurred_ = false;
  DispatchEvent(XRSessionEvent::Create(EventTypeNames::focus, this));
}

void XRSession::OnBlur() {
  if (blurred_)
    return;

  blurred_ = true;
  DispatchEvent(XRSessionEvent::Create(EventTypeNames::blur, this));
}

void XRSession::OnFrame(
    std::unique_ptr<TransformationMatrix> base_pose_matrix) {
  DVLOG(2) << __FUNCTION__;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  // Don't allow frames to be processed if there's no layers attached to the
  // session. That would allow tracking with no associated visuals.
  if (!base_layer_)
    return;

  XRPresentationFrame* presentation_frame = new XRPresentationFrame(this);
  presentation_frame->UpdateBasePose(std::move(base_pose_matrix));

  if (pending_frame_) {
    pending_frame_ = false;

    // Cache the base layer, since it could change during the frame callback.
    XRLayer* frame_base_layer = base_layer_;
    frame_base_layer->OnFrameStart();

    // Resolve the queued requestAnimationFrame callbacks. All XR rendering will
    // happen within these calls. resolving_frame_ will be true for the duration
    // of the callbacks.
    AutoReset<bool> resolving(&resolving_frame_, true);
    callback_collection_.ExecuteCallbacks(this, presentation_frame);

    frame_base_layer->OnFrameEnd();
  }
}

// Called when the canvas element for this session's output context is resized.
void XRSession::UpdateCanvasDimensions(Element* element) {
  DCHECK(element);

  double devicePixelRatio = 1.0;
  LocalFrame* frame = device_->xr()->GetFrame();
  if (frame) {
    devicePixelRatio = frame->DevicePixelRatio();
  }

  views_dirty_ = true;
  output_width_ = element->OffsetWidth() * devicePixelRatio;
  output_height_ = element->OffsetHeight() * devicePixelRatio;

  if (!exclusive_ && base_layer_) {
    base_layer_->OnResize();
  }
}

const HeapVector<Member<XRView>>& XRSession::views() {
  // TODO(bajones): For now we assume that exclusive sessions render a stereo
  // pair of views and non-exclusive sessions render a single view. That doesn't
  // always hold true, however, so the view configuration should ultimately come
  // from the backing service.
  if (views_dirty_) {
    if (exclusive_) {
      // If we don't already have the views allocated, do so now.
      if (views_.IsEmpty()) {
        views_.push_back(new XRView(this, XRView::kEyeLeft));
        views_.push_back(new XRView(this, XRView::kEyeRight));
      }

      // In exclusive mode the projection and view matrices must be aligned with
      // the device's physical optics.
      UpdateViewFromEyeParameters(views_[XRView::kEyeLeft],
                                  device_->xrDisplayInfoPtr()->leftEye,
                                  depth_near_, depth_far_);
      UpdateViewFromEyeParameters(views_[XRView::kEyeRight],
                                  device_->xrDisplayInfoPtr()->rightEye,
                                  depth_near_, depth_far_);
    } else {
      if (views_.IsEmpty()) {
        views_.push_back(new XRView(this, XRView::kEyeLeft));
        views_[XRView::kEyeLeft]->UpdateOffset(0, 0, 0);
      }

      float aspect = 1.0f;
      if (output_width_ && output_height_) {
        aspect = static_cast<float>(output_width_) /
                 static_cast<float>(output_height_);
      }

      // In non-exclusive mode the projection matrix must be aligned with the
      // output canvas dimensions.
      views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromAspect(
          kMagicWindowVerticalFieldOfView, aspect, depth_near_, depth_far_);
    }

    views_dirty_ = false;
  }

  return views_;
}

void XRSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(output_context_);
  visitor->Trace(base_layer_);
  visitor->Trace(views_);
  visitor->Trace(resize_observer_);
  visitor->Trace(callback_collection_);
  EventTargetWithInlineData::Trace(visitor);
}

void XRSession::TraceWrappers(
    const blink::ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(callback_collection_);
  EventTargetWithInlineData::TraceWrappers(visitor);
}

}  // namespace blink
