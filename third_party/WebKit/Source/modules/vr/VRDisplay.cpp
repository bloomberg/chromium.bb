// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplay.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/DOMException.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/ScriptedAnimationController.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/FrameView.h"
#include "core/frame/ImageBitmap.h"
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
#include "platform/UserGestureIndicator.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/AutoReset.h"
#include "wtf/Time.h"

#include <array>

namespace blink {

namespace {

VREye stringToVREye(const String& whichEye) {
  if (whichEye == "left")
    return VREyeLeft;
  if (whichEye == "right")
    return VREyeRight;
  return VREyeNone;
}

}  // namespace

VRDisplay::VRDisplay(NavigatorVR* navigatorVR,
                     device::mojom::blink::VRDisplayPtr display,
                     device::mojom::blink::VRDisplayClientRequest request)
    : ContextLifecycleObserver(navigatorVR->document()),
      m_navigatorVR(navigatorVR),
      m_capabilities(new VRDisplayCapabilities()),
      m_eyeParametersLeft(new VREyeParameters()),
      m_eyeParametersRight(new VREyeParameters()),
      m_display(std::move(display)),
      m_submit_frame_client_binding(this),
      m_displayClientBinding(this, std::move(request)) {}

VRDisplay::~VRDisplay() {}

VRController* VRDisplay::controller() {
  return m_navigatorVR->controller();
}

void VRDisplay::update(const device::mojom::blink::VRDisplayInfoPtr& display) {
  m_displayId = display->index;
  m_displayName = display->displayName;
  m_isConnected = true;

  m_capabilities->setHasOrientation(display->capabilities->hasOrientation);
  m_capabilities->setHasPosition(display->capabilities->hasPosition);
  m_capabilities->setHasExternalDisplay(
      display->capabilities->hasExternalDisplay);
  m_capabilities->setCanPresent(display->capabilities->canPresent);
  m_capabilities->setMaxLayers(display->capabilities->canPresent ? 1 : 0);

  // Ignore non presenting delegate
  bool isValid = display->leftEye->renderWidth > 0;
  bool needOnPresentChange = false;
  if (m_isPresenting && isValid && !m_isValidDeviceForPresenting) {
    needOnPresentChange = true;
  }
  m_isValidDeviceForPresenting = isValid;
  m_eyeParametersLeft->update(display->leftEye);
  m_eyeParametersRight->update(display->rightEye);

  if (!display->stageParameters.is_null()) {
    if (!m_stageParameters)
      m_stageParameters = new VRStageParameters();
    m_stageParameters->update(display->stageParameters);
  } else {
    m_stageParameters = nullptr;
  }

  if (needOnPresentChange) {
    OnPresentChange();
  }
}

void VRDisplay::disconnected() {
  if (m_isConnected)
    m_isConnected = !m_isConnected;
}

bool VRDisplay::getFrameData(VRFrameData* frameData) {
  if (!m_navigatorVR->isFocused() || !m_framePose || m_displayBlurred)
    return false;

  if (!frameData)
    return false;

  if (m_depthNear == m_depthFar)
    return false;

  return frameData->update(m_framePose, m_eyeParametersLeft,
                           m_eyeParametersRight, m_depthNear, m_depthFar);
}

VRPose* VRDisplay::getPose() {
  if (!m_navigatorVR->isFocused() || !m_framePose || m_displayBlurred)
    return nullptr;

  VRPose* pose = VRPose::create();
  pose->setPose(m_framePose);
  return pose;
}

void VRDisplay::resetPose() {
  if (!m_display)
    return;

  m_display->ResetPose();
}

VREyeParameters* VRDisplay::getEyeParameters(const String& whichEye) {
  switch (stringToVREye(whichEye)) {
    case VREyeLeft:
      return m_eyeParametersLeft;
    case VREyeRight:
      return m_eyeParametersRight;
    default:
      return nullptr;
  }
}

int VRDisplay::requestAnimationFrame(FrameRequestCallback* callback) {
  Document* doc = this->document();
  if (!doc)
    return 0;
  m_pendingRaf = true;
  if (!m_vrVSyncProvider.is_bound()) {
    ConnectVSyncProvider();
  } else if (!m_displayBlurred && !m_pendingVsync) {
    m_pendingVsync = true;
    m_vrVSyncProvider->GetVSync(convertToBaseCallback(
        WTF::bind(&VRDisplay::OnVSync, wrapWeakPersistent(this))));
  }
  callback->m_useLegacyTimeBase = false;
  return ensureScriptedAnimationController(doc).registerCallback(callback);
}

void VRDisplay::cancelAnimationFrame(int id) {
  if (!m_scriptedAnimationController)
    return;
  m_scriptedAnimationController->cancelCallback(id);
}

void VRDisplay::OnBlur() {
  m_displayBlurred = true;
  m_vrVSyncProvider.reset();
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayblur, true, false, this, ""));
}

void VRDisplay::OnFocus() {
  m_displayBlurred = false;
  ConnectVSyncProvider();
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayfocus, true, false, this, ""));
}

void ReportPresentationResult(PresentationResult result) {
  // Note that this is called twice for each call to requestPresent -
  // one to declare that requestPresent was called, and one for the
  // result.
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, vrPresentationResultHistogram,
      ("VRDisplayPresentResult",
       static_cast<int>(PresentationResult::PresentationResultMax)));
  vrPresentationResultHistogram.count(static_cast<int>(result));
}

ScriptPromise VRDisplay::requestPresent(ScriptState* scriptState,
                                        const HeapVector<VRLayer>& layers) {
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  UseCounter::count(executionContext, UseCounter::VRRequestPresent);
  if (!executionContext->isSecureContext()) {
    UseCounter::count(executionContext,
                      UseCounter::VRRequestPresentInsecureOrigin);
  }

  ReportPresentationResult(PresentationResult::Requested);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // If the VRDisplay does not advertise the ability to present reject the
  // request.
  if (!m_capabilities->canPresent()) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRDisplay cannot present.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::VRDisplayCannotPresent);
    return promise;
  }

  bool firstPresent = !m_isPresenting;

  // Initiating VR presentation is only allowed in response to a user gesture.
  // If the VRDisplay is already presenting, however, repeated calls are
  // allowed outside a user gesture so that the presented content may be
  // updated.
  if (firstPresent && !UserGestureIndicator::utilizeUserGesture() &&
      !m_inDisplayActivate) {
    DOMException* exception = DOMException::create(
        InvalidStateError, "API can only be initiated by a user gesture.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::NotInitiatedByUserGesture);
    return promise;
  }

  // A valid number of layers must be provided in order to present.
  if (layers.size() == 0 || layers.size() > m_capabilities->maxLayers()) {
    forceExitPresent();
    DOMException* exception =
        DOMException::create(InvalidStateError, "Invalid number of layers.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::InvalidNumberOfLayers);
    return promise;
  }

  // If what we were given has an invalid source, need to exit fullscreen with
  // previous, valid source, so delay m_layer reassignment
  if (layers[0].source().isNull()) {
    forceExitPresent();
    DOMException* exception =
        DOMException::create(InvalidStateError, "Invalid layer source.");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::InvalidLayerSource);
    return promise;
  }
  m_layer = layers[0];

  CanvasRenderingContext* renderingContext;
  if (m_layer.source().isHTMLCanvasElement()) {
    renderingContext =
        m_layer.source().getAsHTMLCanvasElement()->renderingContext();
  } else {
    DCHECK(m_layer.source().isOffscreenCanvas());
    renderingContext =
        m_layer.source().getAsOffscreenCanvas()->renderingContext();
  }

  if (!renderingContext || !renderingContext->is3d()) {
    forceExitPresent();
    DOMException* exception = DOMException::create(
        InvalidStateError, "Layer source must have a WebGLRenderingContext");
    resolver->reject(exception);
    ReportPresentationResult(
        PresentationResult::LayerSourceMissingWebGLContext);
    return promise;
  }

  // Save the WebGL script and underlying GL contexts for use by submitFrame().
  m_renderingContext = toWebGLRenderingContextBase(renderingContext);
  m_contextGL = m_renderingContext->contextGL();

  if ((m_layer.leftBounds().size() != 0 && m_layer.leftBounds().size() != 4) ||
      (m_layer.rightBounds().size() != 0 &&
       m_layer.rightBounds().size() != 4)) {
    forceExitPresent();
    DOMException* exception = DOMException::create(
        InvalidStateError,
        "Layer bounds must either be an empty array or have 4 values");
    resolver->reject(exception);
    ReportPresentationResult(PresentationResult::InvalidLayerBounds);
    return promise;
  }

  if (!m_pendingPresentResolvers.isEmpty()) {
    // If we are waiting on the results of a previous requestPresent call don't
    // fire a new request, just cache the resolver and resolve it when the
    // original request returns.
    m_pendingPresentResolvers.push_back(resolver);
  } else if (firstPresent) {
    bool secureContext = scriptState->getExecutionContext()->isSecureContext();
    if (!m_display) {
      forceExitPresent();
      DOMException* exception = DOMException::create(
          InvalidStateError, "The service is no longer active.");
      resolver->reject(exception);
      return promise;
    }

    m_pendingPresentResolvers.push_back(resolver);
    m_submit_frame_client_binding.Close();
    m_display->RequestPresent(
        secureContext,
        m_submit_frame_client_binding.CreateInterfacePtrAndBind(),
        convertToBaseCallback(
            WTF::bind(&VRDisplay::onPresentComplete, wrapPersistent(this))));
  } else {
    updateLayerBounds();
    resolver->resolve();
    ReportPresentationResult(PresentationResult::SuccessAlreadyPresenting);
  }

  return promise;
}

void VRDisplay::onPresentComplete(bool success) {
  if (success) {
    this->beginPresent();
  } else {
    this->forceExitPresent();
    DOMException* exception = DOMException::create(
        NotAllowedError, "Presentation request was denied.");

    while (!m_pendingPresentResolvers.isEmpty()) {
      ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
      resolver->reject(exception);
    }
  }
}

ScriptPromise VRDisplay::exitPresent(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!m_isPresenting) {
    // Can't stop presenting if we're not presenting.
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRDisplay is not presenting.");
    resolver->reject(exception);
    return promise;
  }

  if (!m_display) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "VRService is not available.");
    resolver->reject(exception);
    return promise;
  }
  m_display->ExitPresent();

  resolver->resolve();

  stopPresenting();

  return promise;
}

void VRDisplay::beginPresent() {
  Document* doc = this->document();
  if (m_capabilities->hasExternalDisplay()) {
    forceExitPresent();
    DOMException* exception = DOMException::create(
        InvalidStateError,
        "VR Presentation not implemented for this VRDisplay.");
    while (!m_pendingPresentResolvers.isEmpty()) {
      ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
      resolver->reject(exception);
    }
    ReportPresentationResult(
        PresentationResult::PresentationNotSupportedByDisplay);
    return;
  } else {
    if (m_layer.source().isHTMLCanvasElement()) {
      // TODO(klausw,crbug.com/698923): suppress compositor updates
      // since they aren't needed, they do a fair amount of extra
      // work.
    } else {
      DCHECK(m_layer.source().isOffscreenCanvas());
      // TODO(junov, crbug.com/695497): Implement OffscreenCanvas presentation
      forceExitPresent();
      DOMException* exception = DOMException::create(
          InvalidStateError, "OffscreenCanvas presentation not implemented.");
      while (!m_pendingPresentResolvers.isEmpty()) {
        ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
        resolver->reject(exception);
      }
      ReportPresentationResult(
          PresentationResult::PresentationNotSupportedByDisplay);
      return;
    }
  }

  if (doc) {
    Platform::current()->recordRapporURL("VR.WebVR.PresentSuccess",
                                         WebURL(doc->url()));
  }

  m_isPresenting = true;
  ReportPresentationResult(PresentationResult::Success);

  updateLayerBounds();

  while (!m_pendingPresentResolvers.isEmpty()) {
    ScriptPromiseResolver* resolver = m_pendingPresentResolvers.takeFirst();
    resolver->resolve();
  }
  OnPresentChange();
}

// Need to close service if exists and then free rendering context.
void VRDisplay::forceExitPresent() {
  if (m_display) {
    m_display->ExitPresent();
  }
  stopPresenting();
}

void VRDisplay::updateLayerBounds() {
  if (!m_display)
    return;

  // Set up the texture bounds for the provided layer
  device::mojom::blink::VRLayerBoundsPtr leftBounds =
      device::mojom::blink::VRLayerBounds::New();
  device::mojom::blink::VRLayerBoundsPtr rightBounds =
      device::mojom::blink::VRLayerBounds::New();

  if (m_layer.leftBounds().size() == 4) {
    leftBounds->left = m_layer.leftBounds()[0];
    leftBounds->top = m_layer.leftBounds()[1];
    leftBounds->width = m_layer.leftBounds()[2];
    leftBounds->height = m_layer.leftBounds()[3];
  } else {
    // Left eye defaults
    leftBounds->left = 0.0f;
    leftBounds->top = 0.0f;
    leftBounds->width = 0.5f;
    leftBounds->height = 1.0f;
    m_layer.setLeftBounds({0.0f, 0.0f, 0.5f, 1.0f});
  }

  if (m_layer.rightBounds().size() == 4) {
    rightBounds->left = m_layer.rightBounds()[0];
    rightBounds->top = m_layer.rightBounds()[1];
    rightBounds->width = m_layer.rightBounds()[2];
    rightBounds->height = m_layer.rightBounds()[3];
  } else {
    // Right eye defaults
    rightBounds->left = 0.5f;
    rightBounds->top = 0.0f;
    rightBounds->width = 0.5f;
    rightBounds->height = 1.0f;
    m_layer.setRightBounds({0.5f, 0.0f, 0.5f, 1.0f});
  }

  m_display->UpdateLayerBounds(m_vrFrameId, std::move(leftBounds),
                               std::move(rightBounds), m_sourceWidth,
                               m_sourceHeight);
}

HeapVector<VRLayer> VRDisplay::getLayers() {
  HeapVector<VRLayer> layers;

  if (m_isPresenting) {
    layers.push_back(m_layer);
  }

  return layers;
}

void VRDisplay::submitFrame() {
  if (!m_display)
    return;
  TRACE_EVENT1("gpu", "submitFrame", "frame", m_vrFrameId);

  Document* doc = this->document();
  if (!m_isPresenting) {
    if (doc) {
      doc->addConsoleMessage(ConsoleMessage::create(
          RenderingMessageSource, WarningMessageLevel,
          "submitFrame has no effect when the VRDisplay is not presenting."));
    }
    return;
  }

  if (!m_inAnimationFrame) {
    if (doc) {
      doc->addConsoleMessage(
          ConsoleMessage::create(RenderingMessageSource, WarningMessageLevel,
                                 "submitFrame must be called within a "
                                 "VRDisplay.requestAnimationFrame callback."));
    }
    return;
  }

  if (!m_contextGL) {
    // Something got confused, we can't submit frames without a GL context.
    return;
  }

  // No frame Id to write before submitting the frame.
  if (m_vrFrameId < 0) {
    // TODO(klausw): There used to be a submitFrame here, but we can't
    // submit without a frameId and associated pose data. Just drop it.
    return;
  }

  m_contextGL->Flush();

  // Check if the canvas got resized, if yes send a bounds update.
  int currentWidth = m_renderingContext->drawingBufferWidth();
  int currentHeight = m_renderingContext->drawingBufferHeight();
  if ((currentWidth != m_sourceWidth || currentHeight != m_sourceHeight) &&
      currentWidth != 0 && currentHeight != 0) {
    m_sourceWidth = currentWidth;
    m_sourceHeight = currentHeight;
    updateLayerBounds();
  }

  // There's two types of synchronization needed for submitting frames:
  //
  // - Before submitting, need to wait for the previous frame to be
  //   pulled off the transfer surface to avoid lost frames. This
  //   is currently a compile-time option, normally we always want
  //   to defer this wait to increase parallelism.
  //
  // - After submitting, need to wait for the mailbox to be consumed,
  //   and must remain inside the execution context while waiting.
  //   The waitForPreviousTransferToFinish option defers this wait
  //   until the next frame. That's more efficient, but seems to
  //   cause wobbly framerates.
  bool waitForPreviousTransferToFinish =
      RuntimeEnabledFeatures::webVRExperimentalRenderingEnabled();

  if (waitForPreviousTransferToFinish) {
    TRACE_EVENT0("gpu", "VRDisplay::waitForPreviousTransferToFinish");
    while (m_pendingSubmitFrame) {
      if (!m_submit_frame_client_binding.WaitForIncomingMethodCall()) {
        DLOG(ERROR) << "Failed to receive SubmitFrame response";
        break;
      }
    }
  }

  RefPtr<Image> imageRef = m_renderingContext->getImage(
      PreferAcceleration, SnapshotReasonCreateImageBitmap);

  // Hardware-accelerated rendering should always be texture backed,
  // as implemented by AcceleratedStaticBitmapImage. Ensure this is
  // the case, don't attempt to render if using an unexpected drawing
  // path.
  if (!imageRef->isTextureBacked()) {
    NOTREACHED() << "WebVR requires hardware-accelerated rendering to texture";
    return;
  }

  // The AcceleratedStaticBitmapImage must be kept alive until the
  // mailbox is used via createAndConsumeTextureCHROMIUM, the mailbox
  // itself does not keep it alive. We must keep a reference to the
  // image until the mailbox was consumed.
  StaticBitmapImage* staticImage =
      static_cast<StaticBitmapImage*>(imageRef.get());
  staticImage->ensureMailbox();

  if (waitForPreviousTransferToFinish) {
    // Save a reference to the image to keep it alive until next frame,
    // where we'll wait for the transfer to finish before overwriting
    // it.
    m_previousImage = std::move(imageRef);
  }

  // Wait for the previous render to finish, to avoid losing frames in the
  // Android Surface / GLConsumer pair. TODO(klausw): make this tunable?
  // Other devices may have different preferences.
  {
    TRACE_EVENT0("gpu", "waitForPreviousRenderToFinish");
    while (m_pendingPreviousFrameRender) {
      if (!m_submit_frame_client_binding.WaitForIncomingMethodCall()) {
        DLOG(ERROR) << "Failed to receive SubmitFrame response";
        break;
      }
    }
  }

  m_pendingPreviousFrameRender = true;
  m_pendingSubmitFrame = true;
  m_display->SubmitFrame(
      m_vrFrameId, gpu::MailboxHolder(staticImage->mailbox(),
                                      staticImage->syncToken(), GL_TEXTURE_2D));

  // If preserveDrawingBuffer is false, must clear now. Normally this
  // happens as part of compositing, but that's not active while
  // presenting, so run the responsible code directly.
  m_renderingContext->markCompositedAndClearBackbufferIfNeeded();

  // If we're not deferring the wait for transferring the mailbox,
  // we need to wait for it now to prevent the image going out of
  // scope before its mailbox is retrieved.
  if (!waitForPreviousTransferToFinish) {
    TRACE_EVENT0("gpu", "waitForCurrentTransferToFinish");
    while (m_pendingSubmitFrame) {
      if (!m_submit_frame_client_binding.WaitForIncomingMethodCall()) {
        DLOG(ERROR) << "Failed to receive SubmitFrame response";
        break;
      }
    }
  }
}

void VRDisplay::OnSubmitFrameTransferred() {
  m_pendingSubmitFrame = false;
}

void VRDisplay::OnSubmitFrameRendered() {
  m_pendingPreviousFrameRender = false;
}

Document* VRDisplay::document() {
  return m_navigatorVR->document();
}

void VRDisplay::OnPresentChange() {
  if (m_isPresenting && !m_isValidDeviceForPresenting) {
    DVLOG(1) << __FUNCTION__ << ": device not valid, not sending event";
    return;
  }
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplaypresentchange, true, false, this, ""));
}

void VRDisplay::OnChanged(device::mojom::blink::VRDisplayInfoPtr display) {
  update(display);
}

void VRDisplay::OnExitPresent() {
  stopPresenting();
}

void VRDisplay::onConnected() {
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayconnect, true, false, this, "connect"));
}

void VRDisplay::onDisconnected() {
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplaydisconnect, true, false, this, "disconnect"));
}

void VRDisplay::stopPresenting() {
  if (m_isPresenting) {
    if (!m_capabilities->hasExternalDisplay()) {
      if (m_layer.source().isHTMLCanvasElement()) {
        // TODO(klausw,crbug.com/698923): If compositor updates are
        // suppressed, restore them here.
      } else {
        // TODO(junov, crbug.com/695497): Implement for OffscreenCanvas
      }
    } else {
      // Can't get into this presentation mode, so nothing to do here.
    }
    m_isPresenting = false;
    OnPresentChange();
  }

  m_renderingContext = nullptr;
  m_contextGL = nullptr;
  m_pendingSubmitFrame = false;
  m_pendingPreviousFrameRender = false;
}

void VRDisplay::OnActivate(device::mojom::blink::VRDisplayEventReason reason) {
  if (!m_navigatorVR->isFocused() || m_displayBlurred)
    return;
  AutoReset<bool> activating(&m_inDisplayActivate, true);
  m_navigatorVR->dispatchVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplayactivate, true, false, this, reason));
}

void VRDisplay::OnDeactivate(
    device::mojom::blink::VRDisplayEventReason reason) {
  m_navigatorVR->enqueueVREvent(VRDisplayEvent::create(
      EventTypeNames::vrdisplaydeactivate, true, false, this, reason));
}

void VRDisplay::processScheduledAnimations(double timestamp) {
  // Check if we still have a valid context, the animation controller
  // or document may have disappeared since we scheduled this.
  Document* doc = this->document();
  if (!doc || m_displayBlurred || !m_scriptedAnimationController)
    return;

  TRACE_EVENT1("gpu", "VRDisplay::OnVSync", "frame", m_vrFrameId);

  AutoReset<bool> animating(&m_inAnimationFrame, true);
  m_pendingRaf = false;

  m_scriptedAnimationController->serviceScriptedAnimations(timestamp);
}

void VRDisplay::OnVSync(device::mojom::blink::VRPosePtr pose,
                        mojo::common::mojom::blink::TimeDeltaPtr time,
                        int16_t frameId,
                        device::mojom::blink::VRVSyncProvider::Status error) {
  switch (error) {
    case device::mojom::blink::VRVSyncProvider::Status::SUCCESS:
      break;
    case device::mojom::blink::VRVSyncProvider::Status::CLOSING:
      return;
  }
  m_pendingVsync = false;

  WTF::TimeDelta timeDelta =
      WTF::TimeDelta::FromMicroseconds(time->microseconds);
  // Ensure a consistent timebase with document rAF.
  if (m_timebase < 0) {
    m_timebase = WTF::monotonicallyIncreasingTime() - timeDelta.InSecondsF();
  }

  m_framePose = std::move(pose);
  m_vrFrameId = frameId;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444. I suspect
  // this is due to WaitForIncomingMethodCall receiving the OnVSync
  // but queueing it for immediate execution since it doesn't match
  // the interface being waited on.
  Platform::current()->currentThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&VRDisplay::processScheduledAnimations,
                wrapWeakPersistent(this), m_timebase + timeDelta.InSecondsF()));
}

void VRDisplay::ConnectVSyncProvider() {
  if (!m_navigatorVR->isFocused() || m_vrVSyncProvider.is_bound())
    return;
  m_display->GetVRVSyncProvider(mojo::MakeRequest(&m_vrVSyncProvider));
  m_vrVSyncProvider.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&VRDisplay::OnVSyncConnectionError, wrapWeakPersistent(this))));
  if (m_pendingRaf && !m_displayBlurred) {
    m_pendingVsync = true;
    m_vrVSyncProvider->GetVSync(convertToBaseCallback(
        WTF::bind(&VRDisplay::OnVSync, wrapWeakPersistent(this))));
  }
}

void VRDisplay::OnVSyncConnectionError() {
  m_vrVSyncProvider.reset();
  ConnectVSyncProvider();
}

ScriptedAnimationController& VRDisplay::ensureScriptedAnimationController(
    Document* doc) {
  if (!m_scriptedAnimationController)
    m_scriptedAnimationController = ScriptedAnimationController::create(doc);

  return *m_scriptedAnimationController;
}

void VRDisplay::dispose() {
  m_displayClientBinding.Close();
  m_vrVSyncProvider.reset();
}

ExecutionContext* VRDisplay::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

const AtomicString& VRDisplay::interfaceName() const {
  return EventTargetNames::VRDisplay;
}

void VRDisplay::contextDestroyed(ExecutionContext*) {
  forceExitPresent();
  m_scriptedAnimationController.clear();
}

bool VRDisplay::hasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners attached to it.
  return getExecutionContext() && hasEventListeners();
}

void VRDisplay::focusChanged() {
  // TODO(mthiesse): Blur/focus the display.
  m_vrVSyncProvider.reset();
  ConnectVSyncProvider();
}

DEFINE_TRACE(VRDisplay) {
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_navigatorVR);
  visitor->trace(m_capabilities);
  visitor->trace(m_stageParameters);
  visitor->trace(m_eyeParametersLeft);
  visitor->trace(m_eyeParametersRight);
  visitor->trace(m_layer);
  visitor->trace(m_renderingContext);
  visitor->trace(m_scriptedAnimationController);
  visitor->trace(m_pendingPresentResolvers);
}

}  // namespace blink
