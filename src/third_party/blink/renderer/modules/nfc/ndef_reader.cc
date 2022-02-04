// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_make_read_only_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_scan_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_write_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reading_event.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

DOMException* NDEFErrorTypeToDOMException(
    device::mojom::blink::NDEFErrorType error_type,
    const String& error_message) {
  switch (error_type) {
    case device::mojom::blink::NDEFErrorType::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, error_message);
    case device::mojom::blink::NDEFErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, error_message);
    case device::mojom::blink::NDEFErrorType::NOT_READABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError, error_message);
    case device::mojom::blink::NDEFErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                error_message);
    case device::mojom::blink::NDEFErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                error_message);
    case device::mojom::blink::NDEFErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                error_message);
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return nullptr;
}

DOMException* NDEFErrorPtrToDOMException(
    device::mojom::blink::NDEFErrorPtr error) {
  return NDEFErrorTypeToDOMException(error->error_type, error->error_message);
}

constexpr char kNotSupportedOrPermissionDenied[] =
    "Web NFC is unavailable or permission denied.";

constexpr char kChildFrameErrorMessage[] =
    "Web NFC can only be accessed in a top-level browsing context.";

constexpr char kScanAbortMessage[] = "The NFC scan operation was cancelled.";

constexpr char kWriteAbortMessage[] = "The NFC write operation was cancelled.";

constexpr char kMakeReadOnlyAbortMessage[] =
    "The NFC make read-only operation was cancelled.";
}  // namespace

// static
NDEFReader* NDEFReader::Create(ExecutionContext* context) {
  context->GetScheduler()->RegisterStickyFeature(
      SchedulingPolicy::Feature::kWebNfc,
      {SchedulingPolicy::DisableBackForwardCache()});
  return MakeGarbageCollected<NDEFReader>(context);
}

NDEFReader::NDEFReader(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context), permission_service_(context) {
  // Call GetNFCProxy to create a proxy. This guarantees no allocation will
  // be needed when calling HasPendingActivity later during gc tracing.
  GetNfcProxy();
}

NDEFReader::~NDEFReader() = default;

const AtomicString& NDEFReader::InterfaceName() const {
  return event_target_names::kNDEFReader;
}

ExecutionContext* NDEFReader::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool NDEFReader::HasPendingActivity() const {
  return GetExecutionContext() && GetNfcProxy()->IsReading(this) &&
         HasEventListeners();
}

// https://w3c.github.io/web-nfc/#the-scan-method
ScriptPromise NDEFReader::scan(ScriptState* script_state,
                               const NDEFScanOptions* options,
                               ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  // 7. If reader.[[Signal]]'s aborted flag is set, then reject p with a
  // "AbortError" DOMException and return p.
  if (options->hasSignal() && options->signal()->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      kScanAbortMessage);
    return ScriptPromise();
  }

  // Reject promise when there's already an ongoing scan.
  if (scan_resolver_ || GetNfcProxy()->IsReading(this)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "A scan() operation is ongoing.");
    return ScriptPromise();
  }

  scan_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::Bind(&NDEFReader::ReadOnRequestPermission, WrapPersistent(this),
                WrapPersistent(options)));
  return scan_resolver_->Promise();
}

void NDEFReader::ReadOnRequestPermission(const NDEFScanOptions* options,
                                         PermissionStatus status) {
  if (!scan_resolver_)
    return;

  if (status != PermissionStatus::GRANTED) {
    scan_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    scan_resolver_.Clear();
    return;
  }

  scan_signal_ = options->getSignalOr(nullptr);
  if (scan_signal_) {
    if (scan_signal_->aborted()) {
      scan_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, kScanAbortMessage));
      scan_resolver_.Clear();
      return;
    }
    scan_signal_->AddAlgorithm(
        WTF::Bind(&NDEFReader::ReadAbort, WrapWeakPersistent(this),
                  WrapWeakPersistent(scan_signal_.Get())));
  }

  GetNfcProxy()->StartReading(
      this,
      WTF::Bind(&NDEFReader::ReadOnRequestCompleted, WrapPersistent(this)));
}

void NDEFReader::ReadOnRequestCompleted(
    device::mojom::blink::NDEFErrorPtr error) {
  if (!scan_resolver_)
    return;

  if (error) {
    scan_resolver_->Reject(NDEFErrorPtrToDOMException(std::move(error)));
  } else {
    scan_resolver_->Resolve();
  }

  scan_resolver_.Clear();
}

void NDEFReader::OnReading(const String& serial_number,
                           const device::mojom::blink::NDEFMessage& message) {
  DCHECK(GetNfcProxy()->IsReading(this));
  DispatchEvent(*MakeGarbageCollected<NDEFReadingEvent>(
      event_type_names::kReading, serial_number,
      MakeGarbageCollected<NDEFMessage>(message)));
}

void NDEFReader::OnReadingError(const String& message) {
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo, message));

  // Dispatch the event as the final step in this method as it may cause script
  // to run that destroys the execution context.
  DispatchEvent(*Event::Create(event_type_names::kReadingerror));
}

void NDEFReader::ContextDestroyed() {
  GetNfcProxy()->StopReading(this);
}

void NDEFReader::ReadAbort(AbortSignal* signal) {
  // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
  // bound to this callback to the one last passed to scan().
  if (scan_signal_ != signal)
    return;

  if (scan_resolver_) {
    scan_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, kScanAbortMessage));
    scan_resolver_.Clear();
  }

  GetNfcProxy()->StopReading(this);
}

// https://w3c.github.io/web-nfc/#writing-content
// https://w3c.github.io/web-nfc/#the-write-method
ScriptPromise NDEFReader::write(ScriptState* script_state,
                                const V8NDEFMessageSource* write_message,
                                const NDEFWriteOptions* options,
                                ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  if (options->hasSignal() && options->signal()->aborted()) {
    // If signal’s aborted flag is set, then reject p with an "AbortError"
    // DOMException and return p.
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      kWriteAbortMessage);
    return ScriptPromise();
  }

  // Step 11.2: Run "create NDEF message", if this throws an exception,
  // reject p with that exception and abort these steps.
  NDEFMessage* ndef_message =
      NDEFMessage::Create(script_state, write_message, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  auto message = device::mojom::blink::NDEFMessage::From(ndef_message);
  DCHECK(message);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  write_requests_.insert(resolver);

  // Add the writer to proxy's writer list for Mojo connection error
  // notification.
  GetNfcProxy()->AddWriter(this);

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::Bind(&NDEFReader::WriteOnRequestPermission, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(options),
                std::move(message)));

  return resolver->Promise();
}

void NDEFReader::WriteOnRequestPermission(
    ScriptPromiseResolver* resolver,
    const NDEFWriteOptions* options,
    device::mojom::blink::NDEFMessagePtr message,
    PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    return;
  }

  write_signal_ = options->getSignalOr(nullptr);
  if (write_signal_) {
    if (write_signal_->aborted()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, kWriteAbortMessage));
      return;
    }
    write_signal_->AddAlgorithm(
        WTF::Bind(&NDEFReader::WriteAbort, WrapWeakPersistent(this),
                  WrapWeakPersistent(write_signal_.Get())));
  }

  auto callback = WTF::Bind(&NDEFReader::WriteOnRequestCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));
  GetNfcProxy()->Push(std::move(message),
                      device::mojom::blink::NDEFWriteOptions::From(options),
                      std::move(callback));
}

void NDEFReader::WriteOnRequestCompleted(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(write_requests_.Contains(resolver));

  write_requests_.erase(resolver);

  if (error.is_null()) {
    resolver->Resolve();
  } else {
    resolver->Reject(NDEFErrorPtrToDOMException(std::move(error)));
  }
}

void NDEFReader::WriteAbort(AbortSignal* signal) {
  // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
  // bound to this callback to the one last passed to write().
  if (write_signal_ != signal)
    return;

  // WriteOnRequestCompleted() should always be called whether the push
  // operation is cancelled successfully or not.
  GetNfcProxy()->CancelPush();
}

ScriptPromise NDEFReader::makeReadOnly(ScriptState* script_state,
                                       const NDEFMakeReadOnlyOptions* options,
                                       ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!DomWindow() || !DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kChildFrameErrorMessage);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  make_read_only_requests_.insert(resolver);

  // Add the writer to proxy's writer list for Mojo connection error
  // notification.
  GetNfcProxy()->AddWriter(this);

  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(PermissionName::NFC),
      LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame()),
      WTF::Bind(&NDEFReader::MakeReadOnlyOnRequestPermission,
                WrapPersistent(this), WrapPersistent(resolver),
                WrapPersistent(options)));

  return resolver->Promise();
}

void NDEFReader::MakeReadOnlyOnRequestPermission(
    ScriptPromiseResolver* resolver,
    const NDEFMakeReadOnlyOptions* options,
    PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "NFC permission request denied."));
    return;
  }

  make_read_only_signal_ = options->getSignalOr(nullptr);
  if (make_read_only_signal_) {
    if (make_read_only_signal_->aborted()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, kMakeReadOnlyAbortMessage));
      return;
    }
    make_read_only_signal_->AddAlgorithm(
        WTF::Bind(&NDEFReader::MakeReadOnlyAbort, WrapWeakPersistent(this),
                  WrapWeakPersistent(make_read_only_signal_.Get())));
  }

  auto callback = WTF::Bind(&NDEFReader::MakeReadOnlyOnRequestCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));
  GetNfcProxy()->MakeReadOnly(std::move(callback));
}

void NDEFReader::MakeReadOnlyOnRequestCompleted(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(make_read_only_requests_.Contains(resolver));

  make_read_only_requests_.erase(resolver);

  if (error.is_null()) {
    resolver->Resolve();
  } else {
    resolver->Reject(NDEFErrorPtrToDOMException(std::move(error)));
  }
}

void NDEFReader::MakeReadOnlyAbort(AbortSignal* signal) {
  // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
  // bound to this callback to the one last passed to makeReadOnly().
  if (make_read_only_signal_ != signal)
    return;

  // MakeReadOnlyOnRequestCompleted() should always be called whether the
  // makeReadOnly operation is cancelled successfully or not.
  GetNfcProxy()->CancelMakeReadOnly();
}

NFCProxy* NDEFReader::GetNfcProxy() const {
  DCHECK(DomWindow());
  return NFCProxy::From(*DomWindow());
}

void NDEFReader::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(scan_resolver_);
  visitor->Trace(scan_signal_);
  visitor->Trace(write_requests_);
  visitor->Trace(write_signal_);
  visitor->Trace(make_read_only_requests_);
  visitor->Trace(make_read_only_signal_);
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

PermissionService* NDEFReader::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return permission_service_.get();
}

void NDEFReader::ReadOnMojoConnectionError() {
  // If |scan_resolver_| has already settled this rejection is silently ignored.
  if (scan_resolver_) {
    scan_resolver_->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
    scan_resolver_.Clear();
  }
}

void NDEFReader::WriteOnMojoConnectionError() {
  // If the mojo connection breaks, All push requests will be rejected with a
  // default error.

  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> write_requests;
  write_requests_.swap(write_requests);
  for (ScriptPromiseResolver* resolver : write_requests) {
    resolver->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
  }
}

void NDEFReader::MakeReadOnlyOnMojoConnectionError() {
  // If the mojo connection breaks, All makeReadOnly requests will be rejected
  // with a default error.

  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> make_read_only_requests;
  make_read_only_requests_.swap(make_read_only_requests);
  for (ScriptPromiseResolver* resolver : make_read_only_requests) {
    resolver->Reject(NDEFErrorTypeToDOMException(
        device::mojom::blink::NDEFErrorType::NOT_SUPPORTED,
        kNotSupportedOrPermissionDenied));
  }
}

}  // namespace blink
