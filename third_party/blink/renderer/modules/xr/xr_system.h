// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SYSTEM_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_dom_overlay_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_session_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptPromiseResolver;
class XRFrameProvider;
class XRSessionInit;

// Implementation of the XRSystem interface according to
// https://immersive-web.github.io/webxr/#xrsystem-interface . This is created
// lazily via the NavigatorXR class on first access to the navigator.xr attrib,
// and disposed when the execution context is destroyed or on mojo communication
// errors with the browser/device process.
//
// When the XRSystem is used for promises, it uses query objects to store state
// including the associated ScriptPromiseResolver. These query objects are owned
// by the XRSystem and remain alive until the promise is resolved or rejected.
// (See comments below for PendingSupportsSessionQuery and
// PendingRequestSessionQuery.) These query objects are destroyed and any
// outstanding promises rejected when the XRSystem is disposed.
//
// The XRSystem owns mojo connections with the Browser process through
// VRService, used for capability queries and session lifetime
// management. The XRSystem is also the receiver for the VRServiceClient.
//
// The XRSystem owns mojo connections with the Device process (either a
// separate utility process, or implemented as part of the Browser process,
// depending on the runtime and options) through XRFrameProvider and
// XREnvironmentIntegrationProvider. These are used to transport per-frame data
// such as image data and input poses. These are lazily created when first
// needed for a sensor-backed session (all except sensorless inline sessions),
// and destroyed when the XRSystem is disposed.
//
// The XRSystem keeps weak references to XRSession objects after they were
// returned through a successful requestSession promise, but does not own them.
class XRSystem final : public EventTargetWithInlineData,
                       public ExecutionContextLifecycleObserver,
                       public device::mojom::blink::VRServiceClient,
                       public FocusChangedObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XRSystem);

 public:
  // TODO(crbug.com/976796): Fix lint errors.
  XRSystem(LocalFrame& frame, int64_t ukm_source_id);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange, kDevicechange)

  ScriptPromise supportsSession(ScriptState*,
                                const String&,
                                ExceptionState& exception_state);
  ScriptPromise isSessionSupported(ScriptState*,
                                   const String&,
                                   ExceptionState& exception_state);
  ScriptPromise requestSession(ScriptState*,
                               const String&,
                               XRSessionInit*,
                               ExceptionState& exception_state);

  XRFrameProvider* frameProvider();

  device::mojom::blink::XREnvironmentIntegrationProvider*
  xrEnvironmentProviderRemote();

  // VRServiceClient overrides.
  void OnDeviceChanged() override;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;
  void Trace(Visitor*) override;

  // FocusChangedObserver overrides.
  void FocusedFrameChanged() override;
  bool IsFrameFocused();

  int64_t GetSourceId() const { return ukm_source_id_; }

  using EnvironmentProviderErrorCallback = base::OnceCallback<void()>;
  // Registers a callback that'll be invoked when mojo invokes a disconnect
  // handler on the underlying XREnvironmentIntegrationProvider remote.
  void AddEnvironmentProviderErrorHandler(
      EnvironmentProviderErrorCallback callback);

  void ExitPresent(base::OnceClosure on_exited);

  void SetFramesThrottled(const XRSession* session, bool throttled);

  base::TimeTicks NavigationStart() const { return navigation_start_; }

  bool IsContextDestroyed() const { return is_context_destroyed_; }

 private:
  enum SensorRequirement {
    kNone,
    kOptional,
    kRequired,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SessionRequestStatus : int {
    // `requestSession` succeeded.
    kSuccess = 0,
    // `requestSession` failed with other (unknown) error.
    kOtherError = 1,
    kMaxValue = kOtherError,
  };

  struct RequestedXRSessionFeatureSet {
    // Set of requested features which are known and valid for the current mode.
    XRSessionFeatureSet valid_features;

    // Whether any requested features were unknown or invalid
    bool invalid_features = false;
  };

  // Encapsulates blink-side `XRSystem::requestSession()` call. It is a wrapper
  // around ScriptPromiseResolver that allows us to add additional logic as
  // certain things related to promise's life cycle happen. Instances are owned
  // by the XRSystem, see outstanding_request_queries_ below.
  class PendingRequestSessionQuery final
      : public GarbageCollected<PendingRequestSessionQuery> {
   public:
    PendingRequestSessionQuery(int64_t ukm_source_id,
                               ScriptPromiseResolver* resolver,
                               device::mojom::blink::XRSessionMode mode,
                               RequestedXRSessionFeatureSet required_features,
                               RequestedXRSessionFeatureSet optional_features);
    virtual ~PendingRequestSessionQuery() = default;

    // Resolves underlying promise with passed in XR session.
    // If metrics are to be recorded for this session, an
    // |XRSessionMetricsRecorded| may be passed in as well.
    void Resolve(
        XRSession* session,
        mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
            metrics_recorder = mojo::NullRemote());

    // Rejects underlying promise with a DOMException.
    // Do not call this with |DOMExceptionCode::kSecurityError|, use
    // |RejectWithSecurityError| for that. If the exception is thrown
    // synchronously, an ExceptionState must be passed in. Otherwise it may be
    // null.
    void RejectWithDOMException(DOMExceptionCode exception_code,
                                const String& message,
                                ExceptionState* exception_state);

    // Rejects underlying promise with a SecurityError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithSecurityError(const String& sanitized_message,
                                 ExceptionState* exception_state);

    // Rejects underlying promise with a TypeError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithTypeError(const String& message,
                             ExceptionState* exception_state);

    device::mojom::blink::XRSessionMode mode() const;
    const XRSessionFeatureSet& RequiredFeatures() const;
    const XRSessionFeatureSet& OptionalFeatures() const;
    bool InvalidRequiredFeatures() const;
    bool InvalidOptionalFeatures() const;

    SensorRequirement GetSensorRequirement() const {
      return sensor_requirement_;
    }

    // Returns underlying resolver's script state.
    ScriptState* GetScriptState() const;

    void SetDOMOverlayElement(Element* element) {
      dom_overlay_element_ = element;
    }
    Element* DOMOverlayElement() { return dom_overlay_element_; }

    virtual void Trace(Visitor*);

   private:
    void ParseSensorRequirement();
    device::mojom::XRSessionFeatureRequestStatus GetFeatureRequestStatus(
        device::mojom::XRSessionFeature feature,
        const XRSession* session) const;
    void ReportRequestSessionResult(
        SessionRequestStatus status,
        XRSession* session = nullptr,
        mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
            metrics_recorder = mojo::NullRemote());

    Member<ScriptPromiseResolver> resolver_;
    const device::mojom::blink::XRSessionMode mode_;
    RequestedXRSessionFeatureSet required_features_;
    RequestedXRSessionFeatureSet optional_features_;
    SensorRequirement sensor_requirement_ = SensorRequirement::kNone;

    const int64_t ukm_source_id_;

    Member<Element> dom_overlay_element_;
    DISALLOW_COPY_AND_ASSIGN(PendingRequestSessionQuery);
  };

  static device::mojom::blink::XRSessionOptionsPtr XRSessionOptionsFromQuery(
      const PendingRequestSessionQuery& query);

  // Encapsulates blink-side `XRSystem::isSessionSupported()` call. It is a
  // wrapper around ScriptPromiseResolver that allows us to add additional logic
  // as certain things related to promise's life cycle happen. Instances are
  // owned by the XRSystem, see outstanding_support_queries_ below.
  class PendingSupportsSessionQuery final
      : public GarbageCollected<PendingSupportsSessionQuery> {
   public:
    PendingSupportsSessionQuery(ScriptPromiseResolver*,
                                device::mojom::blink::XRSessionMode,
                                bool throw_on_unsupported);
    virtual ~PendingSupportsSessionQuery() = default;

    // Resolves underlying promise.
    void Resolve(bool supported, ExceptionState* exception_state = nullptr);

    // Rejects underlying promise with a DOMException.
    // Do not call this with |DOMExceptionCode::kSecurityError|, use
    // |RejectWithSecurityError| for that. If the exception is thrown
    // synchronously, an ExceptionState must be passed in. Otherwise it may be
    // null.
    void RejectWithDOMException(DOMExceptionCode exception_code,
                                const String& message,
                                ExceptionState* exception_state);

    // Rejects underlying promise with a SecurityError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithSecurityError(const String& sanitized_message,
                                 ExceptionState* exception_state);

    // Rejects underlying promise with a TypeError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithTypeError(const String& message,
                             ExceptionState* exception_state);

    bool ThrowOnUnsupported() const { return throw_on_unsupported_; }

    device::mojom::blink::XRSessionMode mode() const;

    virtual void Trace(Visitor*);

   private:
    Member<ScriptPromiseResolver> resolver_;
    const device::mojom::blink::XRSessionMode mode_;

    // Only set when calling the deprecated supportsSession method.
    const bool throw_on_unsupported_ = false;

    DISALLOW_COPY_AND_ASSIGN(PendingSupportsSessionQuery);
  };

  // Native event listener for fullscreen change / error events when starting an
  // immersive-ar session that uses DOM Overlay mode. See
  // OnRequestSessionReturned().
  class OverlayFullscreenEventManager : public NativeEventListener {
   public:
    OverlayFullscreenEventManager(
        XRSystem* xr,
        XRSystem::PendingRequestSessionQuery*,
        device::mojom::blink::RequestSessionResultPtr);
    ~OverlayFullscreenEventManager() override;

    // NativeEventListener
    void Invoke(ExecutionContext*, Event*) override;

    void RequestFullscreen();
    void OnSessionStarting();

    void Trace(Visitor*) override;

   private:
    Member<XRSystem> xr_;
    Member<PendingRequestSessionQuery> query_;
    device::mojom::blink::RequestSessionResultPtr result_;
    DISALLOW_COPY_AND_ASSIGN(OverlayFullscreenEventManager);
  };

  // Native event listener used when waiting for fullscreen mode to fully exit
  // when ending an XR session.
  class OverlayFullscreenExitObserver : public NativeEventListener {
   public:
    explicit OverlayFullscreenExitObserver(XRSystem* xr);
    ~OverlayFullscreenExitObserver() override;

    // NativeEventListener
    void Invoke(ExecutionContext*, Event*) override;

    void ExitFullscreen(Element* element, base::OnceClosure on_exited);

    void Trace(Visitor*) override;

   private:
    Member<XRSystem> xr_;
    Member<Element> element_;
    base::OnceClosure on_exited_;
    DISALLOW_COPY_AND_ASSIGN(OverlayFullscreenExitObserver);
  };

  // Helper, logs message to the console as well as DVLOGs.
  void AddConsoleMessage(mojom::blink::ConsoleMessageLevel error_level,
                         const String& message);

  ScriptPromise InternalIsSessionSupported(ScriptState*,
                                           const String&,
                                           ExceptionState& exception_state,
                                           bool throw_on_unsupported);

  const char* CheckInlineSessionRequestAllowed(
      LocalFrame* frame,
      const PendingRequestSessionQuery& query);

  RequestedXRSessionFeatureSet ParseRequestedFeatures(
      Document* doc,
      const HeapVector<ScriptValue>& features,
      const device::mojom::blink::XRSessionMode& session_mode,
      XRSessionInit* session_init,
      mojom::ConsoleMessageLevel error_level);

  void RequestImmersiveSession(LocalFrame* frame,
                               Document* doc,
                               PendingRequestSessionQuery* query,
                               ExceptionState* exception_state);

  void RequestInlineSession(LocalFrame* frame,
                            PendingRequestSessionQuery* query,
                            ExceptionState* exception_state);

  void OnRequestSessionSetupForDomOverlay(
      PendingRequestSessionQuery*,
      device::mojom::blink::RequestSessionResultPtr result);
  void OnRequestSessionReturned(
      PendingRequestSessionQuery*,
      device::mojom::blink::RequestSessionResultPtr result);
  void OnSupportsSessionReturned(PendingSupportsSessionQuery*,
                                 bool supports_session);
  void ResolveSessionRequest(
      PendingRequestSessionQuery*,
      device::mojom::blink::RequestSessionResultPtr result);
  void RejectSessionRequest(PendingRequestSessionQuery*);

  void EnsureDevice();
  void ReportImmersiveSupported(bool supported);

  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

  XRSession* CreateSession(
      device::mojom::blink::XRSessionMode mode,
      XRSession::EnvironmentBlendMode blend_mode,
      XRSession::InteractionMode interaction_mode,
      mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
          client_receiver,
      device::mojom::blink::VRDisplayInfoPtr display_info,
      bool uses_input_eventing,
      XRSessionFeatureSet enabled_features,
      bool sensorless_session = false);

  XRSession* CreateSensorlessInlineSession();

  enum class DisposeType {
    kContextDestroyed = 0,
    kDisconnected = 1,
  };
  void Dispose(DisposeType);

  void OnEnvironmentProviderDisconnect();

  void TryEnsureService();

  // Indicates whether use of requestDevice has already been logged.
  bool did_log_supports_immersive_ = false;

  // Indicates whether we've already logged a request for an immersive session.
  bool did_log_request_immersive_session_ = false;

  const int64_t ukm_source_id_;

  // The XR object owns outstanding pending session queries, these live until
  // the underlying promise is either resolved or rejected.
  HeapHashSet<Member<PendingSupportsSessionQuery>> outstanding_support_queries_;
  HeapHashSet<Member<PendingRequestSessionQuery>> outstanding_request_queries_;
  bool has_outstanding_immersive_request_ = false;

  Vector<EnvironmentProviderErrorCallback>
      environment_provider_error_callbacks_;

  Member<XRFrameProvider> frame_provider_;
  HeapHashSet<WeakMember<XRSession>> sessions_;
  HeapMojoRemote<device::mojom::blink::VRService> service_;
  HeapMojoAssociatedRemote<
      device::mojom::blink::XREnvironmentIntegrationProvider,
      HeapMojoWrapperMode::kWithoutContextObserver>
      environment_provider_;
  HeapMojoReceiver<device::mojom::blink::VRServiceClient, XRSystem> receiver_;

  // Time at which navigation started. Used as the base for relative timestamps,
  // such as for Gamepad objects.
  base::TimeTicks navigation_start_;

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  // In DOM overlay mode, use a fullscreen event listener to detect when
  // transition to fullscreen mode completes or fails, and reject/resolve
  // the pending request session promise accordingly.
  Member<OverlayFullscreenEventManager> fullscreen_event_manager_;
  // DOM overlay mode uses a separate temporary fullscreen event listener
  // if it needs to wait for fullscreen mode to fully exit when ending
  // the session.
  Member<OverlayFullscreenExitObserver> fullscreen_exit_observer_;

  // In DOM overlay mode, save and restore the FrameView background color.
  Color original_base_background_color_;

  bool is_context_destroyed_ = false;
  bool did_service_ever_disconnect_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SYSTEM_H_
