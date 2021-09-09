// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_supported_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/mediastream/capture_handle_config.h"
#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediastream/input_device_info.h"
#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/navigator_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"display-capture\" is disallowed by permission "
    "policy.";

class PromiseResolverCallbacks final : public UserMediaRequest::Callbacks {
 public:
  explicit PromiseResolverCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ~PromiseResolverCallbacks() override = default;

  void OnSuccess(ScriptWrappable* callback_this_value,
                 MediaStream* stream) override {
    resolver_->Resolve(stream);
  }
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error) override {
    resolver_->Reject(error);
  }
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void OnError(ScriptWrappable* callback_this_value,
               DOMExceptionOrOverconstrainedError error) override {
    resolver_->Reject(error);
  }
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    UserMediaRequest::Callbacks::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace

const char MediaDevices::kSupplementName[] = "MediaDevices";

MediaDevices* MediaDevices::mediaDevices(Navigator& navigator) {
  MediaDevices* supplement =
      Supplement<Navigator>::From<MediaDevices>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<MediaDevices>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

MediaDevices::MediaDevices(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      ExecutionContextLifecycleObserver(navigator.DomWindow()),
      stopped_(false),
      receiver_(this, navigator.DomWindow()) {}

MediaDevices::~MediaDevices() = default;

ScriptPromise MediaDevices::enumerateDevices(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  UpdateWebRTCMethodCount(RTCAPIName::kEnumerateDevices);
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  requests_.insert(resolver);

  LocalFrame* frame = LocalDOMWindow::From(script_state)->GetFrame();
  GetDispatcherHost(frame)->EnumerateDevices(
      true /* audio input */, true /* video input */, true /* audio output */,
      true /* request_video_input_capabilities */,
      true /* request_audio_input_capabilities */,
      WTF::Bind(&MediaDevices::DevicesEnumerated, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

MediaTrackSupportedConstraints* MediaDevices::getSupportedConstraints() const {
  return MediaTrackSupportedConstraints::Create();
}

ScriptPromise MediaDevices::getUserMedia(ScriptState* script_state,
                                         const MediaStreamConstraints* options,
                                         ExceptionState& exception_state) {
  return SendUserMediaRequest(script_state,
                              UserMediaRequest::MediaType::kUserMedia, options,
                              exception_state);
}

ScriptPromise MediaDevices::SendUserMediaRequest(
    ScriptState* script_state,
    UserMediaRequest::MediaType media_type,
    const MediaStreamConstraints* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No media device controller available; "
                                      "is this a detached window?");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* callbacks = MakeGarbageCollected<PromiseResolverCallbacks>(resolver);

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  UserMediaController* user_media = UserMediaController::From(window);
  constexpr IdentifiableSurface::Type surface_type =
      IdentifiableSurface::Type::kMediaDevices_GetUserMedia;
  IdentifiableSurface surface;
  if (IdentifiabilityStudySettings::Get()->IsTypeAllowed(surface_type)) {
    surface = IdentifiableSurface::FromTypeAndToken(
        surface_type, TokenFromConstraints(options));
  }
  MediaErrorState error_state;
  UserMediaRequest* request = UserMediaRequest::Create(
      window, user_media, media_type, options, callbacks, error_state, surface);
  if (!request) {
    DCHECK(error_state.HadException());
    if (error_state.CanGenerateException()) {
      error_state.RaiseException(exception_state);
      return ScriptPromise();
    }
    ScriptPromise rejected_promise = resolver->Promise();
    RecordIdentifiabilityMetric(
        surface, GetExecutionContext(),
        IdentifiabilityBenignStringToken(error_state.GetErrorMessage()));
    resolver->Reject(error_state.CreateError());
    return rejected_promise;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_message);
    return ScriptPromise();
  }
  auto promise = resolver->Promise();
  request->Start();
  return promise;
}

ScriptPromise MediaDevices::getDisplayMedia(
    ScriptState* script_state,
    const MediaStreamConstraints* options,
    ExceptionState& exception_state) {
  return SendUserMediaRequest(script_state,
                              UserMediaRequest::MediaType::kDisplayMedia,
                              options, exception_state);
}

ScriptPromise MediaDevices::getCurrentBrowsingContextMedia(
    ScriptState* script_state,
    const MediaStreamConstraints* options,
    ExceptionState& exception_state) {
  const ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The implementation did not support the requested type of object or "
        "operation.");
    return ScriptPromise();
  }

  // This call should not be possible otherwise, as per the RuntimeEnabled
  // in the IDL.
  CHECK(RuntimeEnabledFeatures::GetCurrentBrowsingContextMediaEnabled(context));

  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kDisplayCapture,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  return SendUserMediaRequest(
      script_state,
      UserMediaRequest::MediaType::kGetCurrentBrowsingContextMedia, options,
      exception_state);
}

void MediaDevices::setCaptureHandleConfig(ScriptState* script_state,
                                          const CaptureHandleConfig* config,
                                          ExceptionState& exception_state) {
  DCHECK(config->hasExposeOrigin());
  DCHECK(config->hasHandle());

  if (config->handle().length() > 1024) {
    exception_state.ThrowTypeError(
        "Handle length exceeds 1024 16-bit characters.");
    return;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Current frame is detached.");
    return;
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window || !window->GetFrame()) {
    return;
  }

  if (window->GetFrame() != window->GetFrame()->Top()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Can only be called from the top-level document.");
  }

  auto config_ptr = mojom::blink::CaptureHandleConfig::New();
  config_ptr->expose_origin = config->exposeOrigin();
  config_ptr->capture_handle = config->handle();
  if (config->permittedOrigins().size() == 1 &&
      config->permittedOrigins()[0] == "*") {
    config_ptr->all_origins_permitted = true;
  } else {
    config_ptr->all_origins_permitted = false;
    config_ptr->permitted_origins.ReserveCapacity(
        config->permittedOrigins().size());
    for (const auto& permitted_origin : config->permittedOrigins()) {
      if (permitted_origin == "*") {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Wildcard only valid in isolation.");
        return;
      }

      scoped_refptr<SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(permitted_origin);
      if (!origin || origin->IsOpaque()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Invalid origin encountered.");
        return;
      }
      config_ptr->permitted_origins.emplace_back(std::move(origin));
    }
  }

  GetDispatcherHost(window->GetFrame())
      ->SetCaptureHandleConfig(std::move(config_ptr));
}

const AtomicString& MediaDevices::InterfaceName() const {
  return event_target_names::kMediaDevices;
}

ExecutionContext* MediaDevices::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void MediaDevices::RemoveAllEventListeners() {
  EventTargetWithInlineData::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

void MediaDevices::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  StartObserving();
}

void MediaDevices::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

bool MediaDevices::HasPendingActivity() const {
  DCHECK(stopped_ || receiver_.is_bound() == HasEventListeners());
  return receiver_.is_bound();
}

void MediaDevices::ContextDestroyed() {
  if (stopped_)
    return;

  stopped_ = true;
  requests_.clear();
  dispatcher_host_.reset();
}

void MediaDevices::OnDevicesChanged(
    mojom::blink::MediaDeviceType type,
    const Vector<WebMediaDeviceInfo>& device_infos) {
  DCHECK(GetExecutionContext());

  if (RuntimeEnabledFeatures::OnDeviceChangeEnabled())
    ScheduleDispatchEvent(Event::Create(event_type_names::kDevicechange));

  if (device_change_test_callback_)
    std::move(device_change_test_callback_).Run();
}

void MediaDevices::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);
  if (dispatch_scheduled_events_task_handle_.IsActive())
    return;

  auto* context = GetExecutionContext();
  DCHECK(context);
  dispatch_scheduled_events_task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      WTF::Bind(&MediaDevices::DispatchScheduledEvents, WrapPersistent(this)));
}

void MediaDevices::DispatchScheduledEvents() {
  if (stopped_)
    return;
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  for (const auto& event : events)
    DispatchEvent(*event);
}

void MediaDevices::StartObserving() {
  if (receiver_.is_bound() || stopped_)
    return;

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window)
    return;

  GetDispatcherHost(window->GetFrame())
      ->AddMediaDevicesListener(true /* audio input */, true /* video input */,
                                true /* audio output */,
                                receiver_.BindNewPipeAndPassRemote(
                                    GetExecutionContext()->GetTaskRunner(
                                        TaskType::kMediaElementEvent)));
}

void MediaDevices::StopObserving() {
  if (!receiver_.is_bound())
    return;
  receiver_.reset();
}

namespace {

void RecordEnumeratedDevices(ScriptPromiseResolver* resolver,
                             const MediaDeviceInfoVector& media_devices) {
  if (!IdentifiabilityStudySettings::Get()->IsWebFeatureAllowed(
          WebFeature::kIdentifiabilityMediaDevicesEnumerateDevices)) {
    return;
  }
  Document* document = LocalDOMWindow::From(resolver->GetScriptState())
                           ->GetFrame()
                           ->GetDocument();
  IdentifiableTokenBuilder builder;
  for (const auto& device_info : media_devices) {
    // Ignore device_id since that varies per-site.
    builder.AddToken(IdentifiabilityBenignStringToken(device_info->kind()));
    builder.AddToken(IdentifiabilityBenignStringToken(device_info->label()));
    // Ignore group_id since that is varies per-site.
  }
  IdentifiabilityMetricBuilder(document->UkmSourceID())
      .SetWebfeature(WebFeature::kIdentifiabilityMediaDevicesEnumerateDevices,
                     builder.GetToken())
      .Record(document->UkmRecorder());
}

}  // namespace

void MediaDevices::DevicesEnumerated(
    ScriptPromiseResolver* resolver,
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  if (!requests_.Contains(resolver))
    return;

  requests_.erase(resolver);

  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  DCHECK_EQ(static_cast<wtf_size_t>(
                mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES),
            enumeration.size());

  if (!video_input_capabilities.IsEmpty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT)]
                  .size(),
              video_input_capabilities.size());
  }
  if (!audio_input_capabilities.IsEmpty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
                  .size(),
              audio_input_capabilities.size());
  }

  MediaDeviceInfoVector media_devices;
  for (wtf_size_t i = 0;
       i < static_cast<wtf_size_t>(
               mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES);
       ++i) {
    for (wtf_size_t j = 0; j < enumeration[i].size(); ++j) {
      mojom::blink::MediaDeviceType device_type =
          static_cast<mojom::blink::MediaDeviceType>(i);
      WebMediaDeviceInfo device_info = enumeration[i][j];
      String device_label = String::FromUTF8(device_info.label);
      if (device_label.Contains("AirPods")) {
        device_label = "AirPods";
      }
      if (device_type == mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT ||
          device_type == mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT) {
        InputDeviceInfo* input_device_info =
            MakeGarbageCollected<InputDeviceInfo>(
                String::FromUTF8(device_info.device_id), device_label,
                String::FromUTF8(device_info.group_id), device_type);
        if (device_type == mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT &&
            !video_input_capabilities.IsEmpty()) {
          input_device_info->SetVideoInputCapabilities(
              std::move(video_input_capabilities[j]));
        }
        if (device_type == mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT &&
            !audio_input_capabilities.IsEmpty()) {
          input_device_info->SetAudioInputCapabilities(
              std::move(audio_input_capabilities[j]));
        }
        media_devices.push_back(input_device_info);
      } else {
        media_devices.push_back(MakeGarbageCollected<MediaDeviceInfo>(
            String::FromUTF8(device_info.device_id), device_label,
            String::FromUTF8(device_info.group_id), device_type));
      }
    }
  }

  RecordEnumeratedDevices(resolver, media_devices);

  if (enumerate_devices_test_callback_)
    std::move(enumerate_devices_test_callback_).Run(media_devices);

  resolver->Resolve(media_devices);
}

void MediaDevices::OnDispatcherHostConnectionError() {
  for (ScriptPromiseResolver* resolver : requests_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "enumerateDevices() failed."));
  }
  requests_.clear();
  dispatcher_host_.reset();

  if (connection_error_test_callback_)
    std::move(connection_error_test_callback_).Run();
}

const mojo::Remote<mojom::blink::MediaDevicesDispatcherHost>&
MediaDevices::GetDispatcherHost(LocalFrame* frame) {
  if (!dispatcher_host_) {
    frame->GetBrowserInterfaceBroker().GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver());
    dispatcher_host_.set_disconnect_handler(
        WTF::Bind(&MediaDevices::OnDispatcherHostConnectionError,
                  WrapWeakPersistent(this)));
  }

  return dispatcher_host_;
}

void MediaDevices::SetDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>
        dispatcher_host) {
  dispatcher_host_.Bind(std::move(dispatcher_host));
  dispatcher_host_.set_disconnect_handler(
      WTF::Bind(&MediaDevices::OnDispatcherHostConnectionError,
                WrapWeakPersistent(this)));
}

void MediaDevices::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(scheduled_events_);
  visitor->Trace(requests_);
  Supplement<Navigator>::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
