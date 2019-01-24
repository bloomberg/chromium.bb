// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_device.h"
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

const char kNoOutputContext[] =
    "Inline sessions must be created with an outputContext.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

const char kNoDevicesMessage[] = "No XR hardware found.";

// Helper method to convert IDL options into Mojo options.
device::mojom::blink::XRSessionOptionsPtr convertIdlOptionsToMojo(
    const XRSessionCreationOptions& options) {
  auto session_options = device::mojom::blink::XRSessionOptions::New();
  session_options->immersive = options.immersive();
  session_options->environment_integration = options.environmentIntegration();

  return session_options;
}

}  // namespace

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

const char* XR::checkSessionSupport(
    const XRSessionCreationOptions* options) const {
  if (!options->immersive()) {
    // Validation for non-immersive sessions. Validation for immersive sessions
    // happens browser side.
    if (!options->hasOutputContext()) {
      return kNoOutputContext;
    }
  }

  return nullptr;
}

ScriptPromise XR::supportsSession(ScriptState* script_state,
                                  const XRSessionCreationOptions* options) {
  // Only called from XRDevice, and as such device_ must be initialized first.
  DCHECK(device_);

  // Check to see if the device is capable of supporting the requested session
  // options. Note that reporting support here does not guarantee that creating
  // a session with those options will succeed, as other external and
  // time-sensitve factors (focus state, existence of another immersive session,
  // etc.) may prevent the creation of a session as well.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           reject_reason));
  }

  // If the above checks pass, resolve without a value. Future API iterations
  // may specify a value to be returned here.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertIdlOptionsToMojo(*options);

  device_->SupportsSession(
      std::move(session_options),
      WTF::Bind(&XR::OnSupportsSessionReturned, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

ScriptPromise XR::requestSession(ScriptState* script_state,
                                 const XRSessionCreationOptions* options) {
  // Only called from XRDevice, and as such device_ must be initialized first.
  DCHECK(device_);

  Document* doc = To<Document>(ExecutionContext::From(script_state));

  if (options->immersive() && !did_log_request_immersive_session_ && doc) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  // Check first to see if the device is capable of supporting the requested
  // options.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           reject_reason));
  }

  // TODO(ijamardo): Should we just exit if there is not document?
  bool has_user_activation =
      LocalFrame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr);

  // Check if the current page state prevents the requested session from being
  // created.
  if (options->immersive()) {
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
  if (options->environmentIntegration()) {
    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertIdlOptionsToMojo(*options);
  session_options->has_user_activation = has_user_activation;

  XRPresentationContext* output_context =
      options->hasOutputContext() ? options->outputContext() : nullptr;

  // TODO(http://crbug.com/826899) Once device activation is sorted out for
  // WebXR, either pass in the correct value for metrics to know whether
  // this was triggered by device activation, or remove the value as soon as
  // legacy API has been removed.
  device_->RequestSession(
      std::move(session_options), false /* triggered by display activate */,
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(resolver), WrapPersistent(output_context),
                options->environmentIntegration(), options->immersive()));
  return promise;
}

ScriptPromise XR::requestDevice(ScriptState* script_state) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    // Reject if the frame is inaccessible.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNavigatorDetachedError));
  }

  if (!did_log_requestDevice_ && frame->GetDocument()) {
    ukm::builders::XR_WebXR(ukm_source_id_)
        .SetDidRequestAvailableDevices(1)
        .Record(frame->GetDocument()->UkmRecorder());
    did_log_requestDevice_ = true;
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

  // If we're still waiting for a previous call to resolve return that promise
  // again.
  if (pending_devices_resolver_) {
    return pending_devices_resolver_->Promise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If we no longer have a valid service connection reject the request.
  if (!service_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kNoDevicesMessage));
    return promise;
  }

  // If we already have a device, use that.
  if (xr_device_) {
    resolver->Resolve(xr_device_);
    return promise;
  }

  // Otherwise wait for device request callback.
  pending_devices_resolver_ = resolver;

  // If we're waiting for sync, then request device is already underway.
  if (pending_device_) {
    return promise;
  }

  EnsureDevice();

  return promise;
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
    xr_device_ = MakeGarbageCollected<XRDevice>(this);
  }
  ResolveRequestDevice();
}

// Called when details for every connected XRDevice has been received.
void XR::ResolveRequestDevice() {
  if (pending_devices_resolver_) {
    if (!device_) {
      pending_devices_resolver_->Reject(DOMException::Create(
          DOMExceptionCode::kNotFoundError, kNoDevicesMessage));
    } else {
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

      // Return the device.
      pending_devices_resolver_->Resolve(xr_device_);
    }

    pending_devices_resolver_ = nullptr;
  }
}

void XR::OnSupportsSessionReturned(ScriptPromiseResolver* resolver,
                                   bool supports_session) {
  supports_session
      ? resolver->Resolve()
      : resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
}

void XR::OnRequestSessionReturned(
    ScriptPromiseResolver* resolver,
    XRPresentationContext* output_context,
    bool environment_integration,
    bool immersive,
    device::mojom::blink::XRSessionPtr session_ptr) {
  // TODO(https://crbug.com/872316) Improve the error messaging to indicate why
  // a request failed.
  if (!session_ptr) {
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported);
    resolver->Reject(exception);
    return;
  }

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
      this, std::move(session_ptr->client_request), immersive,
      environment_integration, output_context, blend_mode);
  session->SetXRDisplayInfo(std::move(session_ptr->display_info));
  sessions_.insert(session);

  if (immersive) {
    frameProvider()->BeginImmersiveSession(session, std::move(session_ptr));
  } else {
    magic_window_provider_.Bind(std::move(session_ptr->data_provider));
    if (environment_integration) {
      // See https://bit.ly/2S0zRAS for task types.
      magic_window_provider_->GetEnvironmentIntegrationProvider(
          mojo::MakeRequest(&environment_provider_,
                            GetExecutionContext()->GetTaskRunner(
                                TaskType::kMiscPlatformAPI)));
    }
  }

  resolver->Resolve(session);
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
      binding_.Bind(mojo::MakeRequest(&client, task_runner));
      service_->SetClient(std::move(client));
    }

    // Make sure we have an active device to listen for changes with.
    EnsureDevice();
  }
};

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

  // Ensure that any outstanding requestDevice promises are resolved. They will
  // receive a null result.
  ResolveRequestDevice();
}

void XR::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_device_);
  visitor->Trace(pending_devices_resolver_);
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
