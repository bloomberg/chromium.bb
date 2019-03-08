// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session.h"

#include "base/auto_reset.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_result.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"
#include "third_party/blink/renderer/modules/xr/xr_ray.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_options.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state_init.h"
#include "third_party/blink/renderer/modules/xr/xr_session_event.h"
#include "third_party/blink/renderer/modules/xr/xr_stationary_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_unbounded_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

namespace {

const char kSessionEnded[] = "XRSession has already ended.";

const char kUnknownReferenceSpace[] = "Unknown reference space type.";

const char kSubtypeRequired[] =
    "Subtype must be specified when requesting a stationary reference space.";

const char kReferenceSpaceNotSupported[] =
    "This device does not support the requested reference space type.";

const char kIncompatibleLayer[] =
    "XRLayer was created with a different session.";

const char kNoSpaceSpecified[] = "No XRSpace specified.";

const char kHitTestNotSupported[] = "Device does not support hit-test!";

const char kDeviceDisconnected[] = "The XR device has been disconnected.";

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

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(session_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<XRSession> session_;
};

XRSession::SessionMode XRSession::stringToSessionMode(
    const String& mode_string) {
  if (mode_string == "inline") {
    return kModeInline;
  }
  if (mode_string == "legacy-inline-ar") {
    return kModeInlineAR;
  }
  if (mode_string == "immersive-vr") {
    return kModeImmersiveVR;
  }
  if (mode_string == "immersive-ar") {
    return kModeImmersiveAR;
  }

  return kModeUnknown;
}

String XRSession::sessionModeToString(XRSession::SessionMode mode) {
  if (mode == kModeInline) {
    return "inline";
  }
  if (mode == kModeInlineAR) {
    return "legacy-inline-ar";
  }
  if (mode == kModeImmersiveVR) {
    return "immersive-vr";
  }
  if (mode == kModeImmersiveAR) {
    return "immersive-ar";
  }

  return "";
}

XRSession::XRSession(
    XR* xr,
    device::mojom::blink::XRSessionClientRequest client_request,
    XRSession::SessionMode mode,
    EnvironmentBlendMode environment_blend_mode)
    : xr_(xr),
      mode_(mode),
      mode_string_(sessionModeToString(mode)),
      environment_integration_(mode == kModeInlineAR ||
                               mode == kModeImmersiveAR),
      client_binding_(this, std::move(client_request)),
      callback_collection_(
          MakeGarbageCollected<XRFrameRequestCallbackCollection>(
              xr_->GetExecutionContext())) {
  render_state_ = MakeGarbageCollected<XRRenderState>();
  blurred_ = !HasAppropriateFocus();

  switch (environment_blend_mode) {
    case kBlendModeOpaque:
      blend_mode_string_ = "opaque";
      break;
    case kBlendModeAdditive:
      blend_mode_string_ = "additive";
      break;
    case kBlendModeAlphaBlend:
      blend_mode_string_ = "alpha-blend";
      break;
    default:
      NOTREACHED() << "Unknown environment blend mode: "
                   << environment_blend_mode;
  }
}

bool XRSession::immersive() const {
  return mode_ == kModeImmersiveVR || mode_ == kModeImmersiveAR;
}

void XRSession::SetNonImmersiveProjectionMatrix(
    const WTF::Vector<float>& projection_matrix) {
  DCHECK_EQ(projection_matrix.size(), 16lu);

  non_immersive_projection_matrix_ = projection_matrix;
  // It is about as expensive to check equality as to just
  // update the views, so just update.
  update_views_next_frame_ = true;
}

ExecutionContext* XRSession::GetExecutionContext() const {
  return xr_->GetExecutionContext();
}

const AtomicString& XRSession::InterfaceName() const {
  return event_target_names::kXRSession;
}

void XRSession::updateRenderState(XRRenderStateInit* init,
                                  ExceptionState& exception_state) {
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return;
  }

  if (init->hasBaseLayer() && init->baseLayer()) {
    // Validate that any baseLayer provided was created with this session.
    if (init->baseLayer()->session() != this) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kIncompatibleLayer);
      return;
    }

    // If the baseLayer was previously null and there are outstanding rAF
    // callbacks, kick off a new frame request to flush them out.
    if (!render_state_->baseLayer() && !pending_frame_ &&
        !callback_collection_->IsEmpty()) {
      // Kick off a request for a new XR frame.
      xr_->frameProvider()->RequestFrame(this);
      pending_frame_ = true;
    }
  }

  if (!immersive() && init->hasOutputContext() && init->outputContext()) {
    // If the outputContext was previously null and there are outstanding rAF
    // callbacks, kick off a new frame request to flush them out.
    if (!render_state_->outputContext() && !pending_frame_ &&
        !callback_collection_->IsEmpty()) {
      // Kick off a request for a new XR frame.
      xr_->frameProvider()->RequestFrame(this);
      pending_frame_ = true;
    }
  }

  pending_render_state_.push_back(init);
}

void XRSession::UpdateDisplayInfo(
    const device::mojom::blink::VREyeParametersPtr& left_eye,
    const device::mojom::blink::VREyeParametersPtr& right_eye) {
  auto display_info = display_info_.Clone();
  display_info->leftEye = left_eye.Clone();
  display_info->rightEye = right_eye.Clone();
  SetXRDisplayInfo(std::move(display_info));
}

ScriptPromise XRSession::requestReferenceSpace(
    ScriptState* script_state,
    const XRReferenceSpaceOptions* options) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSessionEnded));
  }

  XRReferenceSpace* reference_space = nullptr;
  if (options->type() == "identity") {
    reference_space = MakeGarbageCollected<XRReferenceSpace>(this);
  } else if (options->type() == "stationary") {
    if (!options->hasSubtype()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kNotSupportedError,
                               kSubtypeRequired));
    }

    XRStationaryReferenceSpace::Subtype subtype;

    if (options->subtype() == "eye-level") {
      subtype = XRStationaryReferenceSpace::kSubtypeEyeLevel;
    } else if (options->subtype() == "floor-level") {
      subtype = XRStationaryReferenceSpace::kSubtypeFloorLevel;
    } else if (options->subtype() == "position-disabled") {
      subtype = XRStationaryReferenceSpace::kSubtypePositionDisabled;
    } else {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kNotSupportedError,
                               kSubtypeRequired));
    }

    reference_space =
        MakeGarbageCollected<XRStationaryReferenceSpace>(this, subtype);
  } else if (options->type() == "bounded") {
    // TODO(https://crbug.com/917411): Bounded reference spaces cannot be
    // returned unless they have bounds geometry. Until we implement that they
    // will be considered unsupported.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           kReferenceSpaceNotSupported));
  } else if (options->type() == "unbounded") {
    if (immersive() && environment_integration_) {
      reference_space = MakeGarbageCollected<XRUnboundedReferenceSpace>(this);
    } else {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kNotSupportedError,
                               kReferenceSpaceNotSupported));
    }
  }

  if (!reference_space) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           kUnknownReferenceSpace));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(reference_space);

  return promise;
}

int XRSession::requestAnimationFrame(V8XRFrameRequestCallback* callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  // Don't allow any new frame requests once the session is ended.
  if (ended_)
    return 0;

  int id = callback_collection_->RegisterCallback(callback);
  if (!pending_frame_) {
    // Kick off a request for a new XR frame.
    xr_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
  return id;
}

void XRSession::cancelAnimationFrame(int id) {
  callback_collection_->CancelCallback(id);
}

HeapVector<Member<XRInputSource>> XRSession::getInputSources() const {
  Document* doc = To<Document>(GetExecutionContext());
  if (!did_log_getInputSources_ && doc) {
    ukm::builders::XR_WebXR(xr_->GetSourceId())
        .SetDidGetXRInputSources(1)
        .Record(doc->UkmRecorder());
    did_log_getInputSources_ = true;
  }

  HeapVector<Member<XRInputSource>> source_array;
  for (const auto& input_source : input_sources_.Values()) {
    source_array.push_back(input_source);
  }

  if (canvas_input_provider_) {
    XRInputSource* input_source = canvas_input_provider_->GetInputSource();
    if (input_source) {
      source_array.push_back(input_source);
    }
  }

  return source_array;
}

ScriptPromise XRSession::requestHitTest(ScriptState* script_state,
                                        XRRay* ray,
                                        XRSpace* space) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSessionEnded));
  }

  if (!space) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNoSpaceSpecified));
  }

  // TODO(https://crbug.com/846411): use space.

  // Reject the promise if device doesn't support the hit-test API.
  // TODO(https://crbug.com/878936): Get the environment provider without going
  // up to xr_, since it doesn't know which runtime's environment provider
  // we want.
  if (!xr_->xrEnvironmentProviderPtr()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           kHitTestNotSupported));
  }

  device::mojom::blink::XRRayPtr ray_mojo = device::mojom::blink::XRRay::New();

  ray_mojo->origin = gfx::mojom::blink::Point3F::New();
  ray_mojo->origin->x = ray->origin()->x();
  ray_mojo->origin->y = ray->origin()->y();
  ray_mojo->origin->z = ray->origin()->z();

  ray_mojo->direction = gfx::mojom::blink::Vector3dF::New();
  ray_mojo->direction->x = ray->direction()->x();
  ray_mojo->direction->y = ray->direction()->y();
  ray_mojo->direction->z = ray->direction()->z();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  EnsureEnvironmentErrorHandler();
  xr_->xrEnvironmentProviderPtr()->RequestHitTest(
      std::move(ray_mojo),
      WTF::Bind(&XRSession::OnHitTestResults, WrapPersistent(this),
                WrapPersistent(resolver)));
  hit_test_promises_.insert(resolver);

  return promise;
}

void XRSession::OnHitTestResults(
    ScriptPromiseResolver* resolver,
    base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>> results) {
  DCHECK(hit_test_promises_.Contains(resolver));
  hit_test_promises_.erase(resolver);

  if (!results) {
    resolver->Reject();
    return;
  }

  HeapVector<Member<XRHitResult>> hit_results;
  for (const auto& mojom_result : results.value()) {
    XRHitResult* hit_result =
        MakeGarbageCollected<XRHitResult>(TransformationMatrix::Create(
            mojom_result->hit_matrix[0], mojom_result->hit_matrix[1],
            mojom_result->hit_matrix[2], mojom_result->hit_matrix[3],
            mojom_result->hit_matrix[4], mojom_result->hit_matrix[5],
            mojom_result->hit_matrix[6], mojom_result->hit_matrix[7],
            mojom_result->hit_matrix[8], mojom_result->hit_matrix[9],
            mojom_result->hit_matrix[10], mojom_result->hit_matrix[11],
            mojom_result->hit_matrix[12], mojom_result->hit_matrix[13],
            mojom_result->hit_matrix[14], mojom_result->hit_matrix[15]));
    hit_results.push_back(hit_result);
  }
  resolver->Resolve(hit_results);
}

void XRSession::EnsureEnvironmentErrorHandler() {
  if (!environment_error_handler_subscribed_ &&
      xr_->xrEnvironmentProviderPtr()) {
    environment_error_handler_subscribed_ = true;
    xr_->AddEnvironmentProviderErrorHandler(WTF::Bind(
        &XRSession::OnEnvironmentProviderError, WrapPersistent(this)));
  }
}

void XRSession::OnEnvironmentProviderError() {
  HeapHashSet<Member<ScriptPromiseResolver>> hit_test_promises;
  hit_test_promises_.swap(hit_test_promises);
  for (ScriptPromiseResolver* resolver : hit_test_promises) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          kDeviceDisconnected));
  }
}

ScriptPromise XRSession::end(ScriptState* script_state) {
  // Don't allow a session to end twice.
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSessionEnded));
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
  // Detach this session from the XR system.
  ended_ = true;
  pending_frame_ = false;

  if (canvas_input_provider_) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }

  // If this session is the active immersive session, notify the frameProvider
  // that it's ended.
  if (xr_->frameProvider()->immersive_session() == this) {
    xr_->frameProvider()->OnImmersiveSessionEnded();
  }

  DispatchEvent(*XRSessionEvent::Create(event_type_names::kEnd, this));
}

double XRSession::NativeFramebufferScale() const {
  if (immersive()) {
    double scale = display_info_->webxr_default_framebuffer_scale;
    DCHECK(scale);

    // Return the inverse of the default scale, since that's what we'll need to
    // multiply the default size by to get back to the native size.
    return 1.0 / scale;
  }
  return 1.0;
}

DoubleSize XRSession::DefaultFramebufferSize() const {
  if (!immersive()) {
    return OutputCanvasSize();
  }

  double width = (display_info_->leftEye->renderWidth +
                  display_info_->rightEye->renderWidth);
  double height = std::max(display_info_->leftEye->renderHeight,
                           display_info_->rightEye->renderHeight);

  double scale = display_info_->webxr_default_framebuffer_scale;
  return DoubleSize(width * scale, height * scale);
}

DoubleSize XRSession::OutputCanvasSize() const {
  if (!render_state_->outputContext()) {
    return DoubleSize();
  }

  return DoubleSize(output_width_, output_height_);
}

XRPresentationContext* XRSession::outputContext() const {
  return render_state_->outputContext();
}

void XRSession::OnFocus() {
  if (!blurred_)
    return;

  blurred_ = false;
  DispatchEvent(*XRSessionEvent::Create(event_type_names::kFocus, this));
}

void XRSession::OnBlur() {
  if (blurred_)
    return;

  blurred_ = true;
  DispatchEvent(*XRSessionEvent::Create(event_type_names::kBlur, this));
}

// Immersive sessions may still not be blurred in headset even if the page isn't
// focused.  This prevents the in-headset experience from freezing on an
// external display headset when the user clicks on another tab.
bool XRSession::HasAppropriateFocus() {
  return immersive() ? has_xr_focus_ : has_xr_focus_ && xr_->IsFrameFocused();
}

void XRSession::OnFocusChanged() {
  if (HasAppropriateFocus()) {
    OnFocus();
  } else {
    OnBlur();
  }
}

void XRSession::DetachOutputContext(XRPresentationContext* output_context) {
  if (!output_context)
    return;

  // Remove anything in this session observing the given output context.
  HTMLCanvasElement* canvas = output_context->canvas();
  if (canvas) {
    if (resize_observer_) {
      resize_observer_->unobserve(canvas);
    }

    if (canvas_input_provider_ && canvas_input_provider_->canvas() == canvas) {
      canvas_input_provider_->Stop();
      canvas_input_provider_ = nullptr;
    }
  }

  if (render_state_->outputContext() == output_context) {
    render_state_->removeOutputContext();
  }
}

void XRSession::ApplyPendingRenderState() {
  if (pending_render_state_.size() > 0) {
    XRLayer* prev_base_layer = render_state_->baseLayer();
    XRPresentationContext* prev_output_context = render_state_->outputContext();
    update_views_next_frame_ = true;

    // Loop through each pending render state and apply it to the active one.
    for (auto& init : pending_render_state_) {
      render_state_->Update(init);
    }
    pending_render_state_.clear();

    // If this is an inline session and the base layer has changed, give it an
    // opportunity to update it's drawing buffer size.
    if (!immersive() && render_state_->baseLayer() &&
        render_state_->baseLayer() != prev_base_layer) {
      render_state_->baseLayer()->OnResize();
    }

    // If the output context changed, remove listeners from the old one and add
    // listeners to the new one as appropriate.
    if (render_state_->outputContext() != prev_output_context) {
      // If we had an output context previously remove anything observing it.
      DetachOutputContext(prev_output_context);

      // When a new output context is provided, monitor it for resize events.
      if (render_state_->outputContext()) {
        render_state_->outputContext()->BindToSession(this);
        HTMLCanvasElement* canvas = render_state_->outputContext()->canvas();
        if (canvas) {
          if (!resize_observer_) {
            resize_observer_ = ResizeObserver::Create(
                canvas->GetDocument(),
                MakeGarbageCollected<XRSessionResizeObserverDelegate>(this));
          }
          resize_observer_->observe(canvas);

          // Begin processing input events on the output context's canvas.
          if (!immersive()) {
            canvas_input_provider_ =
                MakeGarbageCollected<XRCanvasInputProvider>(this, canvas);
          }

          // Get the new canvas dimensions
          UpdateCanvasDimensions(canvas);
        }
      }
    }
  }
}

void XRSession::OnFrame(
    double timestamp,
    std::unique_ptr<TransformationMatrix> base_pose_matrix,
    const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
    const base::Optional<gpu::MailboxHolder>& background_mailbox_holder,
    const base::Optional<IntSize>& background_size) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  base_pose_matrix_ = std::move(base_pose_matrix);

  // If there are pending render state changes, apply them now.
  ApplyPendingRenderState();

  if (pending_frame_) {
    pending_frame_ = false;

    // Don't allow frames to be processed if there's no layers attached to the
    // session. That would allow tracking with no associated visuals.
    XRLayer* frame_base_layer = render_state_->baseLayer();
    if (!frame_base_layer)
      return;

    // Don't allow frames to be processed if an inline session doesn't have an
    // output context.
    if (!immersive() && !render_state_->outputContext())
      return;

    XRFrame* presentation_frame = CreatePresentationFrame();

    // Make sure that any frame-bounded changed to the views array take effect.
    if (update_views_next_frame_) {
      views_dirty_ = true;
      update_views_next_frame_ = false;
    }

    frame_base_layer->OnFrameStart(output_mailbox_holder);

    // TODO(836349): revisit sending background image data to blink at all.
    if (background_mailbox_holder) {
      // If using a background image, the caller must provide its pixel size
      // also. The source size can differ from the current drawing buffer size.
      DCHECK(background_size);
      frame_base_layer->HandleBackgroundImage(background_mailbox_holder.value(),
                                              background_size.value());
    }

    // Resolve the queued requestAnimationFrame callbacks. All XR rendering will
    // happen within these calls. resolving_frame_ will be true for the duration
    // of the callbacks.
    base::AutoReset<bool> resolving(&resolving_frame_, true);
    callback_collection_->ExecuteCallbacks(this, timestamp, presentation_frame);

    // The session might have ended in the middle of the frame. Only call
    // OnFrameEnd if it's still valid.
    if (!ended_)
      frame_base_layer->OnFrameEnd();

    // Ensure the XRFrame cannot be used outside the callbacks.
    presentation_frame->Deactivate();
  }
}

void XRSession::LogGetPose() const {
  Document* doc = To<Document>(GetExecutionContext());
  if (!did_log_getViewerPose_ && doc) {
    did_log_getViewerPose_ = true;

    ukm::builders::XR_WebXR(xr_->GetSourceId())
        .SetDidRequestPose(1)
        .Record(doc->UkmRecorder());
  }
}

XRFrame* XRSession::CreatePresentationFrame() {
  XRFrame* presentation_frame = MakeGarbageCollected<XRFrame>(this);
  if (base_pose_matrix_) {
    presentation_frame->SetBasePoseMatrix(*base_pose_matrix_);
  }
  return presentation_frame;
}

// Called when the canvas element for this session's output context is resized.
void XRSession::UpdateCanvasDimensions(Element* element) {
  DCHECK(element);

  double devicePixelRatio = 1.0;
  LocalFrame* frame = xr_->GetFrame();
  if (frame) {
    devicePixelRatio = frame->DevicePixelRatio();
  }

  update_views_next_frame_ = true;
  output_width_ = element->OffsetWidth() * devicePixelRatio;
  output_height_ = element->OffsetHeight() * devicePixelRatio;
  int output_angle = 0;

  // TODO(crbug.com/836948): handle square canvases.
  // TODO(crbug.com/840346): we should not need to use ScreenOrientation here.
  ScreenOrientation* orientation = ScreenOrientation::Create(frame);

  if (orientation) {
    output_angle = orientation->angle();
    DVLOG(2) << __FUNCTION__ << ": got angle=" << output_angle;
  }

  if (xr_->xrEnvironmentProviderPtr()) {
    xr_->xrEnvironmentProviderPtr()->UpdateSessionGeometry(
        IntSize(output_width_, output_height_),
        display::Display::DegreesToRotation(output_angle));
  }

  if (render_state_->baseLayer()) {
    render_state_->baseLayer()->OnResize();
  }
}

void XRSession::OnInputStateChange(
    int16_t frame_id,
    const WTF::Vector<device::mojom::blink::XRInputSourceStatePtr>&
        input_states) {
  bool devices_changed = false;

  // Update any input sources with new state information. Any updated input
  // sources are marked as active.
  for (const auto& input_state : input_states) {
    XRInputSource* input_source = input_sources_.at(input_state->source_id);
    if (!input_source) {
      input_source =
          MakeGarbageCollected<XRInputSource>(this, input_state->source_id);
      input_sources_.Set(input_state->source_id, input_source);
      devices_changed = true;
    }
    input_source->active_frame_id = frame_id;
    UpdateInputSourceState(input_source, input_state);
  }

  // Remove any input sources that are inactive..
  std::vector<uint32_t> inactive_sources;
  for (const auto& input_source : input_sources_.Values()) {
    if (input_source->active_frame_id != frame_id) {
      inactive_sources.push_back(input_source->source_id());
      devices_changed = true;
    }
  }

  if (!inactive_sources.empty()) {
    for (uint32_t source_id : inactive_sources) {
      input_sources_.erase(source_id);
    }
  }

  if (devices_changed) {
    DispatchEvent(
        *XRSessionEvent::Create(event_type_names::kInputsourceschange, this));
  }
}

void XRSession::OnSelectStart(XRInputSource* input_source) {
  // Discard duplicate events
  if (input_source->primary_input_pressed)
    return;

  input_source->primary_input_pressed = true;
  input_source->selection_cancelled = false;

  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSelectstart, input_source);
  DispatchEvent(*event);

  if (event->defaultPrevented())
    input_source->selection_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRSession::OnSelectEnd(XRInputSource* input_source) {
  // Discard duplicate events
  if (!input_source->primary_input_pressed)
    return;

  input_source->primary_input_pressed = false;

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(frame);

  XRInputSourceEvent* event =
      CreateInputSourceEvent(event_type_names::kSelectend, input_source);
  DispatchEvent(*event);

  if (event->defaultPrevented())
    input_source->selection_cancelled = true;

  // Ensure the frame cannot be used outside of the event handler.
  event->frame()->Deactivate();
}

void XRSession::OnSelect(XRInputSource* input_source) {
  // If a select was fired but we had not previously started the selection it
  // indictes a sub-frame or instantanous select event, and we should fire a
  // selectstart prior to the selectend.
  if (!input_source->primary_input_pressed) {
    OnSelectStart(input_source);
  }

  // Make sure we end the selection prior to firing the select event.
  OnSelectEnd(input_source);

  if (!input_source->selection_cancelled) {
    XRInputSourceEvent* event =
        CreateInputSourceEvent(event_type_names::kSelect, input_source);
    DispatchEvent(*event);

    // Ensure the frame cannot be used outside of the event handler.
    event->frame()->Deactivate();
  }
}

void XRSession::OnPoseReset() {
  DispatchEvent(*XRSessionEvent::Create(event_type_names::kResetpose, this));
}

void XRSession::UpdateInputSourceState(
    XRInputSource* input_source,
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if (!input_source || !state)
    return;

  // Update the input source's description if this state update
  // includes them.
  if (state->description) {
    const device::mojom::blink::XRInputSourceDescriptionPtr& desc =
        state->description;

    input_source->SetTargetRayMode(
        static_cast<XRInputSource::TargetRayMode>(desc->target_ray_mode));

    input_source->SetHandedness(
        static_cast<XRInputSource::Handedness>(desc->handedness));

    input_source->SetEmulatedPosition(desc->emulated_position);

    if (desc->pointer_offset && desc->pointer_offset->matrix.has_value()) {
      const WTF::Vector<float>& m = desc->pointer_offset->matrix.value();
      std::unique_ptr<TransformationMatrix> pointer_matrix =
          TransformationMatrix::Create(m[0], m[1], m[2], m[3], m[4], m[5], m[6],
                                       m[7], m[8], m[9], m[10], m[11], m[12],
                                       m[13], m[14], m[15]);
      input_source->SetPointerTransformMatrix(std::move(pointer_matrix));
    }
  }

  if (state->grip && state->grip->matrix.has_value()) {
    const Vector<float>& m = state->grip->matrix.value();
    std::unique_ptr<TransformationMatrix> grip_matrix =
        TransformationMatrix::Create(m[0], m[1], m[2], m[3], m[4], m[5], m[6],
                                     m[7], m[8], m[9], m[10], m[11], m[12],
                                     m[13], m[14], m[15]);
    input_source->SetBasePoseMatrix(std::move(grip_matrix));
  }

  // Handle state change of the primary input, which may fire events
  if (state->primary_input_clicked)
    OnSelect(input_source);

  if (state->primary_input_pressed) {
    OnSelectStart(input_source);
  } else if (input_source->primary_input_pressed) {
    // May get here if the input source was previously pressed but now isn't,
    // but the input source did not set primary_input_clicked to true. We will
    // treat this as a cancelled selection, firing the selectend event so the
    // page stays in sync with the controller state but won't fire the
    // usual select event.
    OnSelectEnd(input_source);
  }
}

XRInputSourceEvent* XRSession::CreateInputSourceEvent(
    const AtomicString& type,
    XRInputSource* input_source) {
  XRFrame* presentation_frame = CreatePresentationFrame();
  return XRInputSourceEvent::Create(type, presentation_frame, input_source);
}

void XRSession::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  SetXRDisplayInfo(std::move(display_info));
}

void XRSession::OnExitPresent() {
  if (immersive()) {
    ForceEnd();
  }
}

void XRSession::SetXRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  display_info_id_++;
  display_info_ = std::move(display_info);
  is_external_ = display_info_->capabilities->hasExternalDisplay;
}

const HeapVector<Member<XRView>>& XRSession::views() {
  // TODO(bajones): For now we assume that immersive sessions render a stereo
  // pair of views and non-immersive sessions render a single view. That doesn't
  // always hold true, however, so the view configuration should ultimately come
  // from the backing service.
  if (views_dirty_) {
    if (immersive()) {
      // If we don't already have the views allocated, do so now.
      if (views_.IsEmpty()) {
        views_.push_back(MakeGarbageCollected<XRView>(this, XRView::kEyeLeft));
        views_.push_back(MakeGarbageCollected<XRView>(this, XRView::kEyeRight));
      }
      // In immersive mode the projection and view matrices must be aligned with
      // the device's physical optics.
      UpdateViewFromEyeParameters(
          views_[XRView::kEyeLeft], display_info_->leftEye,
          render_state_->depthNear(), render_state_->depthFar());
      UpdateViewFromEyeParameters(
          views_[XRView::kEyeRight], display_info_->rightEye,
          render_state_->depthNear(), render_state_->depthFar());
    } else {
      if (views_.IsEmpty()) {
        views_.push_back(MakeGarbageCollected<XRView>(this, XRView::kEyeLeft));
        views_[XRView::kEyeLeft]->UpdateOffset(0, 0, 0);
      }

      float aspect = 1.0f;
      if (output_width_ && output_height_) {
        aspect = static_cast<float>(output_width_) /
                 static_cast<float>(output_height_);
      }

      if (non_immersive_projection_matrix_.size() > 0) {
        views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromRawValues(
            non_immersive_projection_matrix_, render_state_->depthNear(),
            render_state_->depthFar());
      } else {
        // In non-immersive mode, if there is no explicit projection matrix
        // provided, the projection matrix must be aligned with the
        // output canvas dimensions.
        views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromAspect(
            kMagicWindowVerticalFieldOfView, aspect, render_state_->depthNear(),
            render_state_->depthFar());
      }
    }

    views_dirty_ = false;
  } else {
    // TODO(https://crbug.com/836926): views_dirty_ is not working right for
    // AR mode, we're not picking up the change on the right frame. Remove this
    // fallback once that's sorted out.
    DVLOG(2) << __FUNCTION__ << ": FIXME, fallback proj matrix update";
    if (non_immersive_projection_matrix_.size() > 0) {
      views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromRawValues(
          non_immersive_projection_matrix_, render_state_->depthNear(),
          render_state_->depthFar());
    }
  }

  return views_;
}

bool XRSession::HasPendingActivity() const {
  return !callback_collection_->IsEmpty() && !ended_;
}

void XRSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(render_state_);
  visitor->Trace(pending_render_state_);
  visitor->Trace(views_);
  visitor->Trace(input_sources_);
  visitor->Trace(resize_observer_);
  visitor->Trace(canvas_input_provider_);
  visitor->Trace(callback_collection_);
  visitor->Trace(hit_test_promises_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
