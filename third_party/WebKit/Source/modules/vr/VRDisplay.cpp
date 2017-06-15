// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplay.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/DOMException.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/ScriptedAnimationController.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "modules/EventTargetModules.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplayCapabilities.h"
#include "modules/vr/VREyeParameters.h"
#include "modules/vr/VRFrameData.h"
#include "modules/vr/VRLayer.h"
#include "modules/vr/VRPose.h"
#include "modules/vr/VRStageParameters.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"

#include <array>
#include "core/dom/ExecutionContext.h"

namespace blink {

namespace {

VREye StringToVREye(const String& which_eye) {
  if (which_eye == "left")
    return kVREyeLeft;
  if (which_eye == "right")
    return kVREyeRight;
  return kVREyeNone;
}

class VRDisplayFrameRequestCallback : public FrameRequestCallback {
 public:
  explicit VRDisplayFrameRequestCallback(VRDisplay* vr_display)
      : vr_display_(vr_display) {}
  ~VRDisplayFrameRequestCallback() override {}
  void handleEvent(double high_res_time_ms) override {
    vr_display_->OnMagicWindowVSync(high_res_time_ms / 1000.0);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(vr_display_);

    FrameRequestCallback::Trace(visitor);
  }

  Member<VRDisplay> vr_display_;
};

}  // namespace

VRDisplay::VRDisplay(NavigatorVR* navigator_vr,
                     device::mojom::blink::VRDisplayPtr display,
                     device::mojom::blink::VRDisplayClientRequest request)
    : SuspendableObject(navigator_vr->GetDocument()),
      navigator_vr_(navigator_vr),
      capabilities_(new VRDisplayCapabilities()),
      eye_parameters_left_(new VREyeParameters()),
      eye_parameters_right_(new VREyeParameters()),
      display_(std::move(display)),
      submit_frame_client_binding_(this),
      display_client_binding_(this, std::move(request)) {
  SuspendIfNeeded();  // Initialize SuspendabaleObject.
}

VRDisplay::~VRDisplay() {}

void VRDisplay::Suspend() {}

void VRDisplay::Resume() {
  RequestVSync();
}

VRController* VRDisplay::Controller() {
  return navigator_vr_->Controller();
}

void VRDisplay::Update(const device::mojom::blink::VRDisplayInfoPtr& display) {
  display_id_ = display->index;
  display_name_ = display->displayName;
  is_connected_ = true;

  capabilities_->SetHasPosition(display->capabilities->hasPosition);
  capabilities_->SetHasExternalDisplay(
      display->capabilities->hasExternalDisplay);
  capabilities_->SetCanPresent(display->capabilities->canPresent);
  capabilities_->SetMaxLayers(display->capabilities->canPresent ? 1 : 0);

  // Ignore non presenting delegate
  bool is_valid = display->leftEye->renderWidth > 0;
  bool need_on_present_change = false;
  if (is_presenting_ && is_valid && !is_valid_device_for_presenting_) {
    need_on_present_change = true;
  }
  is_valid_device_for_presenting_ = is_valid;
  eye_parameters_left_->Update(display->leftEye);
  eye_parameters_right_->Update(display->rightEye);

  if (!display->stageParameters.is_null()) {
    if (!stage_parameters_)
      stage_parameters_ = new VRStageParameters();
    stage_parameters_->Update(display->stageParameters);
  } else {
    stage_parameters_ = nullptr;
  }

  if (need_on_present_change) {
    OnPresentChange();
  }
}

bool VRDisplay::getFrameData(VRFrameData* frame_data) {
  if (!FocusedOrPresenting() || !frame_pose_ || display_blurred_)
    return false;

  if (!frame_data)
    return false;

  if (depth_near_ == depth_far_)
    return false;

  return frame_data->Update(frame_pose_, eye_parameters_left_,
                            eye_parameters_right_, depth_near_, depth_far_);
}

VREyeParameters* VRDisplay::getEyeParameters(const String& which_eye) {
  switch (StringToVREye(which_eye)) {
    case kVREyeLeft:
      return eye_parameters_left_;
    case kVREyeRight:
      return eye_parameters_right_;
    default:
      return nullptr;
  }
}

void VRDisplay::RequestVSync() {
  DVLOG(2) << __FUNCTION__
           << " start: pending_vrdisplay_raf_=" << pending_vrdisplay_raf_
           << " in_animation_frame_=" << in_animation_frame_
           << " did_submit_this_frame_=" << did_submit_this_frame_;
  if (!pending_vrdisplay_raf_)
    return;
  Document* doc = navigator_vr_->GetDocument();
  if (!doc || !display_)
    return;
  // If we've switched from magic window to presenting, cancel the Document rAF
  // and start the VrPresentationProvider VSync.
  if (is_presenting_ && pending_vsync_id_ != -1) {
    doc->CancelAnimationFrame(pending_vsync_id_);
    pending_vsync_ = false;
    pending_vsync_id_ = -1;
  }
  if (display_blurred_ || pending_vsync_)
    return;

  if (!is_presenting_) {
    display_->GetNextMagicWindowPose(ConvertToBaseCallback(
        WTF::Bind(&VRDisplay::OnMagicWindowPose, WrapWeakPersistent(this))));
    pending_vsync_ = true;
    pending_vsync_id_ =
        doc->RequestAnimationFrame(new VRDisplayFrameRequestCallback(this));
    return;
  }

  if (!pending_vrdisplay_raf_)
    return;

  // The logic here is a bit subtle. We get called from one of the following
  // four contexts:
  //
  // (a) from requestAnimationFrame if outside an animating context (i.e. the
  //     first rAF call from inside a getVRDisplays() promise)
  //
  // (b) from requestAnimationFrame in an animating context if the JS code
  //     calls rAF after submitFrame.
  //
  // (c) from submitFrame if that is called after rAF.
  //
  // (d) from ProcessScheduledAnimations if a rAF callback finishes without
  //     submitting a frame.
  //
  // These cases are mutually exclusive which prevents duplicate RequestVSync
  // calls. Case (a) only applies outside an animating context
  // (in_animation_frame_ is false), and (b,c,d) all require an animating
  // context. While in an animating context, submitFrame is called either
  // before rAF (b), after rAF (c), or not at all (d). If rAF isn't called at
  // all, there won't be future frames.

  pending_vsync_ = true;
  vr_presentation_provider_->GetVSync(ConvertToBaseCallback(
      WTF::Bind(&VRDisplay::OnPresentingVSync, WrapWeakPersistent(this))));

  DVLOG(2) << __FUNCTION__ << " done: pending_vsync_=" << pending_vsync_;
}

int VRDisplay::requestAnimationFrame(FrameRequestCallback* callback) {
  DVLOG(2) << __FUNCTION__;
  Document* doc = this->GetDocument();
  if (!doc)
    return 0;
  pending_vrdisplay_raf_ = true;

  // We want to delay the GetVSync call while presenting to ensure it doesn't
  // arrive earlier than frame submission, but other than that we want to call
  // it as early as possible. See comments inside RequestVSync() for more
  // details on the applicable cases.
  if (!in_animation_frame_ || did_submit_this_frame_) {
    RequestVSync();
  }
  callback->use_legacy_time_base_ = false;
  return EnsureScriptedAnimationController(doc).RegisterCallback(callback);
}

void VRDisplay::cancelAnimationFrame(int id) {
  DVLOG(2) << __FUNCTION__;
  if (!scripted_animation_controller_)
    return;
  scripted_animation_controller_->CancelCallback(id);
}

void VRDisplay::OnBlur() {
  DVLOG(1) << __FUNCTION__;
  display_blurred_ = true;
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplayblur, true, false, this, ""));
}

void VRDisplay::OnFocus() {
  DVLOG(1) << __FUNCTION__;
  display_blurred_ = false;
  RequestVSync();

  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplayfocus, true, false, this, ""));
}

void ReportPresentationResult(PresentationResult result) {
  // Note that this is called twice for each call to requestPresent -
  // one to declare that requestPresent was called, and one for the
  // result.
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, vr_presentation_result_histogram,
      ("VRDisplayPresentResult",
       static_cast<int>(PresentationResult::kPresentationResultMax)));
  vr_presentation_result_histogram.Count(static_cast<int>(result));
}

ScriptPromise VRDisplay::requestPresent(ScriptState* script_state,
                                        const HeapVector<VRLayer>& layers) {
  DVLOG(1) << __FUNCTION__;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  UseCounter::Count(execution_context, WebFeature::kVRRequestPresent);
  if (!execution_context->IsSecureContext()) {
    UseCounter::Count(execution_context,
                      WebFeature::kVRRequestPresentInsecureOrigin);
  }

  ReportPresentationResult(PresentationResult::kRequested);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If the VRDisplay does not advertise the ability to present reject the
  // request.
  if (!capabilities_->canPresent()) {
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "VRDisplay cannot present.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kVRDisplayCannotPresent);
    return promise;
  }

  bool first_present = !is_presenting_;

  // Initiating VR presentation is only allowed in response to a user gesture.
  // If the VRDisplay is already presenting, however, repeated calls are
  // allowed outside a user gesture so that the presented content may be
  // updated.
  if (first_present && !UserGestureIndicator::ProcessingUserGesture() &&
      !in_display_activate_) {
    DOMException* exception = DOMException::Create(
        kInvalidStateError, "API can only be initiated by a user gesture.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kNotInitiatedByUserGesture);
    return promise;
  }

  // When we are requesting to start presentation with a user action or the
  // display has activated, record the user action.
  if (first_present &&
      (UserGestureIndicator::ProcessingUserGesture() || in_display_activate_)) {
    Platform::Current()->RecordAction(
        UserMetricsAction("VR.WebVR.requestPresent"));
  }

  // A valid number of layers must be provided in order to present.
  if (layers.size() == 0 || layers.size() > capabilities_->maxLayers()) {
    ForceExitPresent();
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "Invalid number of layers.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kInvalidNumberOfLayers);
    return promise;
  }

  // If what we were given has an invalid source, need to exit fullscreen with
  // previous, valid source, so delay m_layer reassignment
  if (layers[0].source().isNull()) {
    ForceExitPresent();
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "Invalid layer source.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kInvalidLayerSource);
    return promise;
  }
  layer_ = layers[0];

  CanvasRenderingContext* rendering_context;
  if (layer_.source().isHTMLCanvasElement()) {
    rendering_context =
        layer_.source().getAsHTMLCanvasElement()->RenderingContext();
  } else {
    DCHECK(layer_.source().isOffscreenCanvas());
    rendering_context =
        layer_.source().getAsOffscreenCanvas()->RenderingContext();
  }

  if (!rendering_context || !rendering_context->Is3d()) {
    ForceExitPresent();
    DOMException* exception = DOMException::Create(
        kInvalidStateError, "Layer source must have a WebGLRenderingContext");
    resolver->Reject(exception);
    ReportPresentationResult(
        PresentationResult::kLayerSourceMissingWebGLContext);
    return promise;
  }

  // Save the WebGL script and underlying GL contexts for use by submitFrame().
  rendering_context_ = ToWebGLRenderingContextBase(rendering_context);
  context_gl_ = rendering_context_->ContextGL();

  if ((layer_.leftBounds().size() != 0 && layer_.leftBounds().size() != 4) ||
      (layer_.rightBounds().size() != 0 && layer_.rightBounds().size() != 4)) {
    ForceExitPresent();
    DOMException* exception = DOMException::Create(
        kInvalidStateError,
        "Layer bounds must either be an empty array or have 4 values");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kInvalidLayerBounds);
    return promise;
  }

  if (!pending_present_resolvers_.IsEmpty()) {
    // If we are waiting on the results of a previous requestPresent call don't
    // fire a new request, just cache the resolver and resolve it when the
    // original request returns.
    pending_present_resolvers_.push_back(resolver);
  } else if (first_present) {
    bool secure_context =
        ExecutionContext::From(script_state)->IsSecureContext();
    if (!display_) {
      ForceExitPresent();
      DOMException* exception = DOMException::Create(
          kInvalidStateError, "The service is no longer active.");
      resolver->Reject(exception);
      return promise;
    }

    pending_present_resolvers_.push_back(resolver);
    device::mojom::blink::VRSubmitFrameClientPtr submit_frame_client;
    submit_frame_client_binding_.Close();
    submit_frame_client_binding_.Bind(mojo::MakeRequest(&submit_frame_client));
    display_->RequestPresent(
        secure_context, std::move(submit_frame_client),
        mojo::MakeRequest(&vr_presentation_provider_),
        ConvertToBaseCallback(
            WTF::Bind(&VRDisplay::OnPresentComplete, WrapPersistent(this))));
    vr_presentation_provider_.set_connection_error_handler(
        ConvertToBaseCallback(
            WTF::Bind(&VRDisplay::OnPresentationProviderConnectionError,
                      WrapWeakPersistent(this))));
    pending_present_request_ = true;
  } else {
    UpdateLayerBounds();
    resolver->Resolve();
    ReportPresentationResult(PresentationResult::kSuccessAlreadyPresenting);
  }

  return promise;
}

void VRDisplay::OnPresentComplete(bool success) {
  pending_present_request_ = false;
  if (success) {
    this->BeginPresent();
  } else {
    this->ForceExitPresent();
    DOMException* exception = DOMException::Create(
        kNotAllowedError, "Presentation request was denied.");

    while (!pending_present_resolvers_.IsEmpty()) {
      ScriptPromiseResolver* resolver = pending_present_resolvers_.TakeFirst();
      resolver->Reject(exception);
    }
  }
}

ScriptPromise VRDisplay::exitPresent(ScriptState* script_state) {
  DVLOG(1) << __FUNCTION__;
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!is_presenting_) {
    // Can't stop presenting if we're not presenting.
    DOMException* exception = DOMException::Create(
        kInvalidStateError, "VRDisplay is not presenting.");
    resolver->Reject(exception);
    return promise;
  }

  if (!display_) {
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "VRService is not available.");
    resolver->Reject(exception);
    return promise;
  }
  display_->ExitPresent();

  resolver->Resolve();

  StopPresenting();

  return promise;
}

void VRDisplay::BeginPresent() {
  Document* doc = this->GetDocument();
  if (capabilities_->hasExternalDisplay()) {
    ForceExitPresent();
    DOMException* exception = DOMException::Create(
        kInvalidStateError,
        "VR Presentation not implemented for this VRDisplay.");
    while (!pending_present_resolvers_.IsEmpty()) {
      ScriptPromiseResolver* resolver = pending_present_resolvers_.TakeFirst();
      resolver->Reject(exception);
    }
    ReportPresentationResult(
        PresentationResult::kPresentationNotSupportedByDisplay);
    return;
  } else {
    if (layer_.source().isHTMLCanvasElement()) {
      // TODO(klausw,crbug.com/698923): suppress compositor updates
      // since they aren't needed, they do a fair amount of extra
      // work.
    } else {
      DCHECK(layer_.source().isOffscreenCanvas());
      // TODO(junov, crbug.com/695497): Implement OffscreenCanvas presentation
      ForceExitPresent();
      DOMException* exception = DOMException::Create(
          kInvalidStateError, "OffscreenCanvas presentation not implemented.");
      while (!pending_present_resolvers_.IsEmpty()) {
        ScriptPromiseResolver* resolver =
            pending_present_resolvers_.TakeFirst();
        resolver->Reject(exception);
      }
      ReportPresentationResult(
          PresentationResult::kPresentationNotSupportedByDisplay);
      return;
    }
  }

  if (doc) {
    Platform::Current()->RecordRapporURL("VR.WebVR.PresentSuccess",
                                         WebURL(doc->Url()));
  }

  is_presenting_ = true;
  // Call RequestVSync to switch from the (internal) document rAF to the
  // VrPresentationProvider VSync.
  RequestVSync();
  ReportPresentationResult(PresentationResult::kSuccess);

  UpdateLayerBounds();

  while (!pending_present_resolvers_.IsEmpty()) {
    ScriptPromiseResolver* resolver = pending_present_resolvers_.TakeFirst();
    resolver->Resolve();
  }
  OnPresentChange();

  // For GVR, we shut down normal vsync processing during VR presentation.
  // Run window.rAF once manually so that applications get a chance to
  // schedule a VRDisplay.rAF in case they do so only while presenting.
  if (!pending_vrdisplay_raf_ && !capabilities_->hasExternalDisplay()) {
    double timestamp = WTF::MonotonicallyIncreasingTime();
    Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, WTF::Bind(&VRDisplay::ProcessScheduledWindowAnimations,
                                   WrapWeakPersistent(this), timestamp));
  }
}

// Need to close service if exists and then free rendering context.
void VRDisplay::ForceExitPresent() {
  if (display_) {
    display_->ExitPresent();
  }
  StopPresenting();
}

void VRDisplay::UpdateLayerBounds() {
  if (!display_)
    return;

  // Left eye defaults
  if (layer_.leftBounds().size() != 4)
    layer_.setLeftBounds({0.0f, 0.0f, 0.5f, 1.0f});
  // Right eye defaults
  if (layer_.rightBounds().size() != 4)
    layer_.setRightBounds({0.5f, 0.0f, 0.5f, 1.0f});

  const Vector<float>& left = layer_.leftBounds();
  const Vector<float>& right = layer_.rightBounds();

  vr_presentation_provider_->UpdateLayerBounds(
      vr_frame_id_, WebFloatRect(left[0], left[1], left[2], left[3]),
      WebFloatRect(right[0], right[1], right[2], right[3]),
      WebSize(source_width_, source_height_));
}

HeapVector<VRLayer> VRDisplay::getLayers() {
  HeapVector<VRLayer> layers;

  if (is_presenting_) {
    layers.push_back(layer_);
  }

  return layers;
}

void VRDisplay::submitFrame() {
  DVLOG(2) << __FUNCTION__;

  if (!display_)
    return;
  TRACE_EVENT1("gpu", "submitFrame", "frame", vr_frame_id_);

  Document* doc = this->GetDocument();
  if (!is_presenting_) {
    if (doc) {
      doc->AddConsoleMessage(ConsoleMessage::Create(
          kRenderingMessageSource, kWarningMessageLevel,
          "submitFrame has no effect when the VRDisplay is not presenting."));
    }
    return;
  }

  if (!in_animation_frame_) {
    if (doc) {
      doc->AddConsoleMessage(
          ConsoleMessage::Create(kRenderingMessageSource, kWarningMessageLevel,
                                 "submitFrame must be called within a "
                                 "VRDisplay.requestAnimationFrame callback."));
    }
    return;
  }

  if (!context_gl_) {
    // Something got confused, we can't submit frames without a GL context.
    return;
  }

  // No frame Id to write before submitting the frame.
  if (vr_frame_id_ < 0) {
    // TODO(klausw): There used to be a submitFrame here, but we can't
    // submit without a frameId and associated pose data. Just drop it.
    return;
  }

  TRACE_EVENT_BEGIN0("gpu", "VRDisplay::Flush_1");
  context_gl_->Flush();
  TRACE_EVENT_END0("gpu", "VRDisplay::Flush_1");

  // Check if the canvas got resized, if yes send a bounds update.
  int current_width = rendering_context_->drawingBufferWidth();
  int current_height = rendering_context_->drawingBufferHeight();
  if ((current_width != source_width_ || current_height != source_height_) &&
      current_width != 0 && current_height != 0) {
    source_width_ = current_width;
    source_height_ = current_height;
    UpdateLayerBounds();
  }

  // There's two types of synchronization needed for submitting frames:
  //
  // - Before submitting, need to wait for the previous frame to be
  //   pulled off the transfer surface to avoid lost frames. This
  //   is currently a compile-time option, normally we always want
  //   to defer this wait to increase parallelism.
  //
  // - After submitting, need to wait for the mailbox to be consumed,
  //   and the image object must remain alive during this time.
  //   We keep a reference to the image so that we can defer this
  //   wait. Here, we wait for the previous transfer to complete.
  {
    TRACE_EVENT0("gpu", "VRDisplay::waitForPreviousTransferToFinish");
    while (pending_submit_frame_) {
      if (!submit_frame_client_binding_.WaitForIncomingMethodCall()) {
        DLOG(ERROR) << "Failed to receive SubmitFrame response";
        break;
      }
    }
  }

  TRACE_EVENT_BEGIN0("gpu", "VRDisplay::GetImage");
  RefPtr<Image> image_ref = rendering_context_->GetImage(
      kPreferAcceleration, kSnapshotReasonCreateImageBitmap);
  TRACE_EVENT_END0("gpu", "VRDisplay::GetImage");

  // Hardware-accelerated rendering should always be texture backed,
  // as implemented by AcceleratedStaticBitmapImage. Ensure this is
  // the case, don't attempt to render if using an unexpected drawing
  // path.
  if (!image_ref.Get() || !image_ref->IsTextureBacked()) {
    NOTREACHED() << "WebVR requires hardware-accelerated rendering to texture";
    return;
  }

  // The AcceleratedStaticBitmapImage must be kept alive until the
  // mailbox is used via createAndConsumeTextureCHROMIUM, the mailbox
  // itself does not keep it alive. We must keep a reference to the
  // image until the mailbox was consumed.
  StaticBitmapImage* static_image =
      static_cast<StaticBitmapImage*>(image_ref.Get());
  TRACE_EVENT_BEGIN0("gpu", "VRDisplay::EnsureMailbox");
  static_image->EnsureMailbox();
  TRACE_EVENT_END0("gpu", "VRDisplay::EnsureMailbox");

  // Save a reference to the image to keep it alive until next frame,
  // where we'll wait for the transfer to finish before overwriting
  // it.
  previous_image_ = std::move(image_ref);

  // Create mailbox and sync token for transfer.
  TRACE_EVENT_BEGIN0("gpu", "VRDisplay::GetMailbox");
  auto mailbox = static_image->GetMailbox();
  TRACE_EVENT_END0("gpu", "VRDisplay::GetMailbox");
  // Flush to avoid black screen flashes which appear to be related to
  // "fence sync must be flushed before generating sync token" GL errors.
  TRACE_EVENT_BEGIN0("gpu", "VRDisplay::Flush_2");
  context_gl_->Flush();
  TRACE_EVENT_END0("gpu", "VRDisplay::Flush_2");
  auto sync_token = static_image->GetSyncToken();

  // Wait for the previous render to finish, to avoid losing frames in the
  // Android Surface / GLConsumer pair. TODO(klausw): make this tunable?
  // Other devices may have different preferences. Do this step as late
  // as possible before SubmitFrame to ensure we can do as much work as
  // possible in parallel with the previous frame's rendering.
  {
    TRACE_EVENT0("gpu", "waitForPreviousRenderToFinish");
    while (pending_previous_frame_render_) {
      if (!submit_frame_client_binding_.WaitForIncomingMethodCall()) {
        DLOG(ERROR) << "Failed to receive SubmitFrame response";
        break;
      }
    }
  }

  pending_previous_frame_render_ = true;
  pending_submit_frame_ = true;

  TRACE_EVENT_BEGIN0("gpu", "VRDisplay::SubmitFrame");
  vr_presentation_provider_->SubmitFrame(
      vr_frame_id_, gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_2D));
  TRACE_EVENT_END0("gpu", "VRDisplay::SubmitFrame");

  did_submit_this_frame_ = true;
  // If we were deferring a rAF-triggered vsync request, do this now.
  RequestVSync();

  // If preserveDrawingBuffer is false, must clear now. Normally this
  // happens as part of compositing, but that's not active while
  // presenting, so run the responsible code directly.
  rendering_context_->MarkCompositedAndClearBackbufferIfNeeded();
}

void VRDisplay::OnSubmitFrameTransferred() {
  DVLOG(3) << __FUNCTION__;
  pending_submit_frame_ = false;
}

void VRDisplay::OnSubmitFrameRendered() {
  DVLOG(3) << __FUNCTION__;
  pending_previous_frame_render_ = false;
}

Document* VRDisplay::GetDocument() {
  return navigator_vr_->GetDocument();
}

void VRDisplay::OnPresentChange() {
  DVLOG(1) << __FUNCTION__ << ": is_presenting_=" << is_presenting_;
  if (is_presenting_ && !is_valid_device_for_presenting_) {
    DVLOG(1) << __FUNCTION__ << ": device not valid, not sending event";
    return;
  }
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplaypresentchange, true, false, this, ""));
}

void VRDisplay::OnChanged(device::mojom::blink::VRDisplayInfoPtr display) {
  Update(display);
}

void VRDisplay::OnExitPresent() {
  StopPresenting();
}

void VRDisplay::OnConnected() {
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplayconnect, true, false, this, "connect"));
}

void VRDisplay::OnDisconnected() {
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplaydisconnect, true, false, this, "disconnect"));
}

void VRDisplay::StopPresenting() {
  if (is_presenting_) {
    if (!capabilities_->hasExternalDisplay()) {
      if (layer_.source().isHTMLCanvasElement()) {
        // TODO(klausw,crbug.com/698923): If compositor updates are
        // suppressed, restore them here.
      } else {
        // TODO(junov, crbug.com/695497): Implement for OffscreenCanvas
      }
    } else {
      // Can't get into this presentation mode, so nothing to do here.
    }
    is_presenting_ = false;

    OnPresentChange();

    // Record user action for stop presenting.  Note that this could be
    // user-triggered or not.
    Platform::Current()->RecordAction(
        UserMetricsAction("VR.WebVR.StopPresenting"));
  }

  rendering_context_ = nullptr;
  context_gl_ = nullptr;
  pending_submit_frame_ = false;
  pending_previous_frame_render_ = false;
  did_submit_this_frame_ = false;
}

void VRDisplay::OnActivate(device::mojom::blink::VRDisplayEventReason reason,
                           OnActivateCallback on_handled) {
  AutoReset<bool> activating(&in_display_activate_, true);
  navigator_vr_->DispatchVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplayactivate, true, false, this, reason));
  std::move(on_handled).Run(!pending_present_request_ && !is_presenting_);
}

void VRDisplay::OnDeactivate(
    device::mojom::blink::VRDisplayEventReason reason) {
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplaydeactivate, true, false, this, reason));
}

void VRDisplay::ProcessScheduledWindowAnimations(double timestamp) {
  TRACE_EVENT1("gpu", "VRDisplay::window.rAF", "frame", vr_frame_id_);
  auto doc = navigator_vr_->GetDocument();
  if (!doc)
    return;
  auto page = doc->GetPage();
  if (!page)
    return;

  bool had_pending_vrdisplay_raf = pending_vrdisplay_raf_;
  // TODO(klausw): update timestamp based on scheduling delay?
  page->Animator().ServiceScriptedAnimations(timestamp);

  if (had_pending_vrdisplay_raf != pending_vrdisplay_raf_) {
    DVLOG(1) << __FUNCTION__
             << ": window.rAF fallback successfully scheduled VRDisplay.rAF";
  }

  if (!pending_vrdisplay_raf_) {
    // There wasn't any call to vrDisplay.rAF, so we will not be getting new
    // frames from now on unless the application schedules one down the road in
    // reaction to a separate event or timeout. TODO(klausw,crbug.com/716087):
    // do something more useful here?
    DVLOG(1) << __FUNCTION__
             << ": no scheduled VRDisplay.requestAnimationFrame, presentation "
                "broken?";
  }
}

void VRDisplay::ProcessScheduledAnimations(double timestamp) {
  DVLOG(2) << __FUNCTION__;
  // Check if we still have a valid context, the animation controller
  // or document may have disappeared since we scheduled this.
  Document* doc = this->GetDocument();
  if (!doc || display_blurred_) {
    DVLOG(2) << __FUNCTION__ << ": early exit, doc=" << doc
             << " display_blurred_=" << display_blurred_;
    return;
  }

  if (doc->IsContextSuspended()) {
    // We are currently suspended - try ProcessScheduledAnimations again later
    // when we resume.
    return;
  }

  TRACE_EVENT1("gpu", "VRDisplay::OnVSync", "frame", vr_frame_id_);

  if (pending_vrdisplay_raf_ && scripted_animation_controller_) {
    // Run the callback, making sure that in_animation_frame_ is only
    // true for the vrDisplay rAF and not for a legacy window rAF
    // that may be called later.
    AutoReset<bool> animating(&in_animation_frame_, true);
    pending_vrdisplay_raf_ = false;
    did_submit_this_frame_ = false;
    scripted_animation_controller_->ServiceScriptedAnimations(timestamp);
    if (pending_vrdisplay_raf_ && !did_submit_this_frame_) {
      DVLOG(2) << __FUNCTION__ << ": vrDisplay.rAF did not submit a frame";
      RequestVSync();
    }
  }
  if (pending_pose_)
    frame_pose_ = std::move(pending_pose_);

  // Sanity check: If pending_vrdisplay_raf_ is true and the vsync provider
  // is connected, we must now have a pending vsync.
  DCHECK(!pending_vrdisplay_raf_ || pending_vsync_);

  // For GVR, we shut down normal vsync processing during VR presentation.
  // Trigger any callbacks on window.rAF manually so that they run after
  // completing the vrDisplay.rAF processing.
  if (is_presenting_ && !capabilities_->hasExternalDisplay()) {
    Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, WTF::Bind(&VRDisplay::ProcessScheduledWindowAnimations,
                                   WrapWeakPersistent(this), timestamp));
  }
}

void VRDisplay::OnPresentingVSync(
    device::mojom::blink::VRPosePtr pose,
    WTF::TimeDelta time_delta,
    int16_t frame_id,
    device::mojom::blink::VRPresentationProvider::VSyncStatus status) {
  DVLOG(2) << __FUNCTION__;
  switch (status) {
    case device::mojom::blink::VRPresentationProvider::VSyncStatus::SUCCESS:
      break;
    case device::mojom::blink::VRPresentationProvider::VSyncStatus::CLOSING:
      return;
  }
  pending_vsync_ = false;

  // Ensure a consistent timebase with document rAF.
  if (timebase_ < 0) {
    timebase_ = WTF::MonotonicallyIncreasingTime() - time_delta.InSecondsF();
  }

  frame_pose_ = std::move(pose);
  vr_frame_id_ = frame_id;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444. I suspect
  // this is due to WaitForIncomingMethodCall receiving the OnVSync
  // but queueing it for immediate execution since it doesn't match
  // the interface being waited on.
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&VRDisplay::ProcessScheduledAnimations,
                WrapWeakPersistent(this), timebase_ + time_delta.InSecondsF()));
}

void VRDisplay::OnMagicWindowVSync(double timestamp) {
  DVLOG(2) << __FUNCTION__;
  pending_vsync_ = false;
  pending_vsync_id_ = -1;
  vr_frame_id_ = -1;
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&VRDisplay::ProcessScheduledAnimations,
                                 WrapWeakPersistent(this), timestamp));
}

void VRDisplay::OnMagicWindowPose(device::mojom::blink::VRPosePtr pose) {
  if (!in_animation_frame_) {
    frame_pose_ = std::move(pose);
  } else {
    pending_pose_ = std::move(pose);
  }
}

void VRDisplay::OnPresentationProviderConnectionError() {
  vr_presentation_provider_.reset();
  ForceExitPresent();
  pending_vsync_ = false;
  RequestVSync();
}

ScriptedAnimationController& VRDisplay::EnsureScriptedAnimationController(
    Document* doc) {
  if (!scripted_animation_controller_)
    scripted_animation_controller_ = ScriptedAnimationController::Create(doc);

  return *scripted_animation_controller_;
}

void VRDisplay::Dispose() {
  display_client_binding_.Close();
  vr_presentation_provider_.reset();
}

ExecutionContext* VRDisplay::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& VRDisplay::InterfaceName() const {
  return EventTargetNames::VRDisplay;
}

void VRDisplay::ContextDestroyed(ExecutionContext* context) {
  SuspendableObject::ContextDestroyed(context);
  ForceExitPresent();
  scripted_animation_controller_.Clear();
}

bool VRDisplay::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners attached to it.
  return GetExecutionContext() && HasEventListeners();
}

void VRDisplay::FocusChanged() {
  DVLOG(1) << __FUNCTION__;
  if (is_presenting_)
    return;
  if (navigator_vr_->IsFocused()) {
    OnFocus();
  } else {
    OnBlur();
  }
}

bool VRDisplay::FocusedOrPresenting() {
  // TODO(mthiesse, crbug.com/687411): Focused state should be determined
  // browser-side to correctly track which display should be receiving input.
  return navigator_vr_->IsFocused() || is_presenting_;
}

DEFINE_TRACE(VRDisplay) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(navigator_vr_);
  visitor->Trace(capabilities_);
  visitor->Trace(stage_parameters_);
  visitor->Trace(eye_parameters_left_);
  visitor->Trace(eye_parameters_right_);
  visitor->Trace(layer_);
  visitor->Trace(rendering_context_);
  visitor->Trace(scripted_animation_controller_);
  visitor->Trace(pending_present_resolvers_);
}

}  // namespace blink
