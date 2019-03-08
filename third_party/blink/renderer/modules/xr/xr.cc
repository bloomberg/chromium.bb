// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr.h"

#include <utility>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char kNavigatorDetachedError[] =
    "The navigator.xr object is no longer associated with a document.";

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"xr\" is disallowed by feature policy.";

const char kActiveImmersiveSession[] =
    "There is already an active, immersive XRSession.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

const char kNoDevicesMessage[] = "No XR hardware found.";

// Helper method to convert session mode into Mojo options.
device::mojom::blink::XRSessionOptionsPtr convertModeToMojo(
    XRSession::SessionMode mode) {
  auto session_options = device::mojom::blink::XRSessionOptions::New();
  session_options->immersive = (mode == XRSession::kModeImmersiveVR ||
                                mode == XRSession::kModeImmersiveAR);
  session_options->environment_integration =
      (mode == XRSession::kModeInlineAR || mode == XRSession::kModeImmersiveAR);

  return session_options;
}

}  // namespace

XR::PendingSessionQuery::PendingSessionQuery(
    ScriptPromiseResolver* resolver,
    XRSession::SessionMode session_mode)
    : resolver(resolver), mode(session_mode) {}

void XR::PendingSessionQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver);
}

XR::XR(LocalFrame& frame, int64_t ukm_source_id)
    : ContextLifecycleObserver(frame.GetDocument()),
      FocusChangedObserver(frame.GetPage()),
      ukm_source_id_(ukm_source_id),
      binding_(this) {
  // See https://bit.ly/2S0zRAS for task types.
  frame.GetInterfaceProvider().GetInterface(mojo::MakeRequest(
      &service_, frame.GetTaskRunner(TaskType::kMiscPlatformAPI)));
  service_.set_connection_error_handler(
      WTF::Bind(&XR::Dispose, WrapWeakPersistent(this)));
}

void XR::FocusedFrameChanged() {
  // Tell all sessions that focus changed.
  for (const auto& session : sessions_) {
    session->OnFocusChanged();
  }

  if (frame_provider_)
    frame_provider_->OnFocusChanged();
}

bool XR::IsFrameFocused() {
  return FocusChangedObserver::IsFrameFocused(GetFrame());
}

ExecutionContext* XR::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& XR::InterfaceName() const {
  return event_target_names::kXR;
}

XRFrameProvider* XR::frameProvider() {
  if (!frame_provider_) {
    frame_provider_ = MakeGarbageCollected<XRFrameProvider>(this);
  }

  return frame_provider_;
}

const device::mojom::blink::XREnvironmentIntegrationProviderAssociatedPtr&
XR::xrEnvironmentProviderPtr() {
  return environment_provider_;
}

void XR::AddEnvironmentProviderErrorHandler(
    EnvironmentProviderErrorCallback callback) {
  environment_provider_error_callbacks_.push_back(std::move(callback));
}

ScriptPromise XR::supportsSessionMode(ScriptState* script_state,
                                      const String& mode) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    // Reject if the frame is inaccessible.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNavigatorDetachedError));
  }

  if (!frame->GetDocument()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kWebVr,
          ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlocked));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (mode == "inline") {
    // `inline` sessions are always supported if not blocked by feature policy.
    resolver->Resolve();
  } else {
    // For all other modes we need to check with the service.
    PendingSessionQuery* query = MakeGarbageCollected<PendingSessionQuery>(
        resolver, XRSession::stringToSessionMode(mode));

    if (!device_) {
      pending_mode_queries_.push_back(query);

      // The pending queries will be resolved once the device is returned.
      EnsureDevice();
    } else {
      DispatchSupportsSessionMode(query);
    }
  }

  return promise;
}

void XR::DispatchSupportsSessionMode(PendingSessionQuery* query) {
  if (!device_) {
    // If we don't have a device by the time we reach this call it indicates
    // that there's no WebXR hardware. Reject as not supported.
    query->resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
    return;
  }

  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertModeToMojo(query->mode);

  device_->SupportsSession(
      std::move(session_options),
      WTF::Bind(&XR::OnSupportsSessionReturned, WrapPersistent(this),
                WrapPersistent(query)));
}

ScriptPromise XR::requestSession(ScriptState* script_state,
                                 const XRSessionCreationOptions* options) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    // Reject if the frame is inaccessible.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNavigatorDetachedError));
  }

  Document* doc = frame->GetDocument();
  bool immersive =
      (options->mode() == "immersive-vr" || options->mode() == "immersive-ar");
  if (immersive && !did_log_request_immersive_session_ && doc) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebVr,
                             ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlocked));
  }

  // If we no longer have a valid service connection reject the request.
  if (!service_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotFoundError,
                                           kNoDevicesMessage));
  }

  // TODO(ijamardo): Should we just exit if there is not document?
  bool has_user_activation =
      LocalFrame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr);

  // Check if the current page state prevents the requested session from being
  // created.
  if (options->mode() == "immersive-vr" || options->mode() == "immersive-ar") {
    if (frameProvider()->immersive_session()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kActiveImmersiveSession));
    }

    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }
  }

  // All AR sessions require a user gesture.
  if (options->mode() == "legacy-inline-ar") {
    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }

    doc->AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, mojom::ConsoleMessageLevel::kWarning,
        "Inline AR is deprecated and will be removed soon."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  PendingSessionQuery* query = MakeGarbageCollected<PendingSessionQuery>(
      resolver, XRSession::stringToSessionMode(options->mode()));
  query->has_user_activation = has_user_activation;

  if (!device_) {
    pending_session_requests_.push_back(query);

    // The pending queries will be resolved once the device is returned.
    EnsureDevice();
  } else {
    DispatchRequestSession(query);
  }

  return promise;
}

void XR::DispatchRequestSession(PendingSessionQuery* query) {
  if (!device_) {
    if (query->mode == XRSession::kModeInline) {
      XRSession* session = MakeGarbageCollected<XRSession>(
          this, nullptr /* client request */, query->mode,
          XRSession::kBlendModeOpaque);
      sessions_.insert(session);
      query->resolver->Resolve(session);
      return;
    }

    // If we don't have a device by the time we reach this call it indicates
    // that there's no WebXR hardware. Reject as not supported.
    query->resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
    return;
  }

  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertModeToMojo(query->mode);
  session_options->has_user_activation = query->has_user_activation;

  // TODO(http://crbug.com/826899) Once device activation is sorted out for
  // WebXR, either pass in the correct value for metrics to know whether
  // this was triggered by device activation, or remove the value as soon as
  // legacy API has been removed.
  device_->RequestSession(
      std::move(session_options), false /* triggered by display activate */,
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));
}

void XR::EnsureDevice() {
  // Exit if we have a device or are waiting for a device.
  if (device_ || pending_device_) {
    return;
  }

  service_->RequestDevice(
      WTF::Bind(&XR::OnRequestDeviceReturned, WrapPersistent(this)));
  pending_device_ = true;
}

// This will be called when the XR hardware or capabilities have potentially
// changed. For example, if a new physical device was connected to the system,
// it might be able to support immersive sessions, where it couldn't before.
void XR::OnDeviceChanged() {
  DispatchEvent(*blink::Event::Create(event_type_names::kDevicechange));
}

void XR::OnRequestDeviceReturned(device::mojom::blink::XRDevicePtr device) {
  pending_device_ = false;
  if (device) {
    device_ = std::move(device);

    // Log metrics
    if (!did_log_returned_device_ || !did_log_supports_immersive_) {
      Document* doc = GetFrame() ? GetFrame()->GetDocument() : nullptr;
      if (doc) {
        ukm::builders::XR_WebXR ukm_builder(ukm_source_id_);
        ukm_builder.SetReturnedDevice(1);
        did_log_returned_device_ = true;

        ukm_builder.Record(doc->UkmRecorder());

        device::mojom::blink::XRSessionOptionsPtr session_options =
            device::mojom::blink::XRSessionOptions::New();
        session_options->immersive = true;

        // TODO(http://crbug.com/872086) This shouldn't need to be called.
        // This information should be logged on the browser side.
        device_->SupportsSession(
            std::move(session_options),
            WTF::Bind(&XR::ReportImmersiveSupported, WrapPersistent(this)));
      }
    }
  }

  DispatchPendingSessionCalls();
}

void XR::DispatchPendingSessionCalls() {
  // Process any calls that were waiting for the device query to be returned.
  for (auto& query : pending_mode_queries_) {
    DispatchSupportsSessionMode(query);
  }
  pending_mode_queries_.clear();

  for (auto& query : pending_session_requests_) {
    DispatchRequestSession(query);
  }
  pending_session_requests_.clear();
}

void XR::OnSupportsSessionReturned(PendingSessionQuery* query,
                                   bool supports_session) {
  supports_session
      ? query->resolver->Resolve()
      : query->resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
}

void XR::OnRequestSessionReturned(
    PendingSessionQuery* query,
    device::mojom::blink::XRSessionPtr session_ptr) {
  // TODO(https://crbug.com/872316) Improve the error messaging to indicate why
  // a request failed.
  if (!session_ptr) {
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported);
    query->resolver->Reject(exception);
    return;
  }

  bool environment_integration = query->mode == XRSession::kModeInlineAR ||
                                 query->mode == XRSession::kModeImmersiveAR;

  // immersive sessions must supply display info.
  DCHECK(session_ptr->display_info);
  // If the session supports environment integration, ensure the device does
  // as well.
  DCHECK(!environment_integration || session_ptr->display_info->capabilities
                                         ->canProvideEnvironmentIntegration);

  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  if (environment_integration)
    blend_mode = XRSession::kBlendModeAlphaBlend;

  XRSession* session = MakeGarbageCollected<XRSession>(
      this, std::move(session_ptr->client_request), query->mode, blend_mode);
  session->SetXRDisplayInfo(std::move(session_ptr->display_info));
  sessions_.insert(session);

  if (query->mode == XRSession::kModeImmersiveVR ||
      query->mode == XRSession::kModeImmersiveAR) {
    frameProvider()->BeginImmersiveSession(session, std::move(session_ptr));
  } else {
    magic_window_provider_.Bind(std::move(session_ptr->data_provider));
    if (environment_integration) {
      // See https://bit.ly/2S0zRAS for task types.
      magic_window_provider_->GetEnvironmentIntegrationProvider(
          mojo::MakeRequest(&environment_provider_,
                            GetExecutionContext()->GetTaskRunner(
                                TaskType::kMiscPlatformAPI)));

      environment_provider_.set_connection_error_handler(WTF::Bind(
          &XR::OnEnvironmentProviderDisconnect, WrapWeakPersistent(this)));
    }
  }

  query->resolver->Resolve(session);
}

void XR::ReportImmersiveSupported(bool supported) {
  Document* doc = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  if (doc && !did_log_supports_immersive_ && supported) {
    ukm::builders::XR_WebXR ukm_builder(ukm_source_id_);
    ukm_builder.SetReturnedPresentationCapableDevice(1);
    ukm_builder.Record(doc->UkmRecorder());
    did_log_supports_immersive_ = true;
  }
}

void XR::AddedEventListener(const AtomicString& event_type,
                            RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);

  if (event_type == event_type_names::kDevicechange) {
    // Register for notifications if we haven't already.
    //
    // See https://bit.ly/2S0zRAS for task types.
    auto task_runner =
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    if (!binding_.is_bound()) {
      device::mojom::blink::VRServiceClientPtr client;
      binding_.Bind(mojo::MakeRequest(&client, task_runner), task_runner);
      service_->SetClient(std::move(client));
    }

    // Make sure we have an active device to listen for changes with.
    EnsureDevice();
  }
}

void XR::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void XR::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  binding_.Close();

  // Shutdown frame provider, which manages the message pipes.
  if (frame_provider_)
    frame_provider_->Dispose();

  device_ = nullptr;
}

void XR::OnEnvironmentProviderDisconnect() {
  for (auto& callback : environment_provider_error_callbacks_) {
    std::move(callback).Run();
  }

  environment_provider_error_callbacks_.clear();
  environment_provider_.reset();
}

void XR::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_mode_queries_);
  visitor->Trace(pending_session_requests_);
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
