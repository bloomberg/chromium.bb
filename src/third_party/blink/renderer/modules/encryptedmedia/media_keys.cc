/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_keys_policy.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/timer.h"

#define MEDIA_KEYS_LOG_LEVEL 3

namespace blink {

// A class holding a pending action.
class MediaKeys::PendingAction final
    : public GarbageCollected<MediaKeys::PendingAction> {
 public:
  using Type = EmeApiType;

  Type GetType() const { return type_; }

  const Persistent<ContentDecryptionModuleResult> Result() const {
    return result_;
  }

  DOMArrayBuffer* Data() const {
    DCHECK_EQ(Type::kSetServerCertificate, type_);
    return data_;
  }

  const String& StringData() const {
    DCHECK_EQ(Type::kGetStatusForPolicy, type_);
    return string_data_;
  }

  static PendingAction* CreatePendingSetServerCertificate(
      ContentDecryptionModuleResult* result,
      DOMArrayBuffer* server_certificate) {
    DCHECK(result);
    DCHECK(server_certificate);
    return MakeGarbageCollected<PendingAction>(
        Type::kSetServerCertificate, result, server_certificate, String());
  }

  static PendingAction* CreatePendingGetStatusForPolicy(
      ContentDecryptionModuleResult* result,
      const String& min_hdcp_version) {
    DCHECK(result);
    return MakeGarbageCollected<PendingAction>(
        Type::kGetStatusForPolicy, result, nullptr, min_hdcp_version);
  }

  PendingAction(Type type,
                ContentDecryptionModuleResult* result,
                DOMArrayBuffer* data,
                const String& string_data)
      : type_(type), result_(result), data_(data), string_data_(string_data) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(result_);
    visitor->Trace(data_);
  }

 private:
  const Type type_;
  const Member<ContentDecryptionModuleResult> result_;
  const Member<DOMArrayBuffer> data_;
  const String string_data_;
};

// This class wraps the promise resolver used when setting the certificate
// and is passed to Chromium to fullfill the promise. This implementation of
// complete() will resolve the promise with true, while completeWithError()
// will reject the promise with an exception. completeWithSession()
// is not expected to be called, and will reject the promise.
class SetCertificateResultPromise
    : public ContentDecryptionModuleResultPromise {
 public:
  SetCertificateResultPromise(ScriptState* script_state, MediaKeys* media_keys)
      : ContentDecryptionModuleResultPromise(script_state,
                                             EmeApiType::kSetServerCertificate),
        media_keys_(media_keys) {}

  ~SetCertificateResultPromise() override = default;

  // ContentDecryptionModuleResult implementation.
  void Complete() override {
    if (!IsValidToFulfillPromise())
      return;

    Resolve(true);
  }

  void CompleteWithError(WebContentDecryptionModuleException exception_code,
                         uint32_t system_code,
                         const WebString& error_message) override {
    if (!IsValidToFulfillPromise())
      return;

    // The EME spec specifies that "If the Key System implementation does
    // not support server certificates, return a promise resolved with
    // false." So convert any NOTSUPPORTEDERROR into resolving with false.
    if (exception_code ==
        kWebContentDecryptionModuleExceptionNotSupportedError) {
      Resolve(false);
      return;
    }

    ContentDecryptionModuleResultPromise::CompleteWithError(
        exception_code, system_code, error_message);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(media_keys_);
    ContentDecryptionModuleResultPromise::Trace(visitor);
  }

 private:
  // Keeping a reference to MediaKeys to prevent GC from collecting it while
  // the promise is pending.
  Member<MediaKeys> media_keys_;
};

// This class wraps the promise resolver used when getting the key status for
// policy and is passed to Chromium to fullfill the promise.
class GetStatusForPolicyResultPromise
    : public ContentDecryptionModuleResultPromise {
 public:
  GetStatusForPolicyResultPromise(ScriptState* script_state,
                                  MediaKeys* media_keys)
      : ContentDecryptionModuleResultPromise(script_state,
                                             EmeApiType::kGetStatusForPolicy),
        media_keys_(media_keys) {}

  ~GetStatusForPolicyResultPromise() override = default;

  // ContentDecryptionModuleResult implementation.
  void CompleteWithKeyStatus(
      WebEncryptedMediaKeyInformation::KeyStatus key_status) override {
    if (!IsValidToFulfillPromise())
      return;

    Resolve(EncryptedMediaUtils::ConvertKeyStatusToString(key_status));
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(media_keys_);
    ContentDecryptionModuleResultPromise::Trace(visitor);
  }

 private:
  // Keeping a reference to MediaKeys to prevent GC from collecting it while
  // the promise is pending.
  Member<MediaKeys> media_keys_;
};

MediaKeys::MediaKeys(
    ExecutionContext* context,
    const WebVector<WebEncryptedMediaSessionType>& supported_session_types,
    std::unique_ptr<WebContentDecryptionModule> cdm)
    : ExecutionContextLifecycleObserver(context),
      supported_session_types_(supported_session_types),
      cdm_(std::move(cdm)),
      media_element_(nullptr),
      reserved_for_media_element_(false),
      timer_(context->GetTaskRunner(TaskType::kMiscPlatformAPI),
             this,
             &MediaKeys::TimerFired) {
  DVLOG(MEDIA_KEYS_LOG_LEVEL) << __func__ << "(" << this << ")";
  InstanceCounters::IncrementCounter(InstanceCounters::kMediaKeysCounter);
}

MediaKeys::~MediaKeys() {
  DVLOG(MEDIA_KEYS_LOG_LEVEL) << __func__ << "(" << this << ")";
  InstanceCounters::DecrementCounter(InstanceCounters::kMediaKeysCounter);
}

MediaKeySession* MediaKeys::createSession(ScriptState* script_state,
                                          const String& session_type_string,
                                          ExceptionState& exception_state) {
  DVLOG(MEDIA_KEYS_LOG_LEVEL)
      << __func__ << "(" << this << ") " << session_type_string;

  // [RuntimeEnabled] does not work with enum values. So we have to check it
  // here. See https://crbug.com/871867 for details.
  if (!RuntimeEnabledFeatures::
          EncryptedMediaPersistentUsageRecordSessionEnabled() &&
      session_type_string == "persistent-usage-record") {
    DVLOG(MEDIA_KEYS_LOG_LEVEL)
        << __func__ << ": 'persistent-usage-record' support not enabled.";
    // The message here is carefully chosen to be exactly the same as what the
    // generated bindings would generate for invalid enum values.
    exception_state.ThrowTypeError(
        "The provided value 'persistent-usage-record' is not a valid enum "
        "value of type MediaKeySessionType.");
    return nullptr;
  }

  // From http://w3c.github.io/encrypted-media/#createSession

  // When this method is invoked, the user agent must run the following steps:
  // 1. If this object's persistent state allowed value is false and
  //    sessionType is not "temporary", throw a new DOMException whose name is
  //    NotSupportedError.
  //    (Chromium ensures that only session types supported by the
  //    configuration are listed in supportedSessionTypes.)
  // 2. If the Key System implementation represented by this object's cdm
  //    implementation value does not support sessionType, throw a new
  //    DOMException whose name is NotSupportedError.
  WebEncryptedMediaSessionType session_type =
      EncryptedMediaUtils::ConvertToSessionType(session_type_string);
  if (!SessionTypeSupported(session_type)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Unsupported session type.");
    return nullptr;
  }

  // 3. Let session be a new MediaKeySession object, and initialize it as
  //    follows:
  //    (Initialization is performed in the constructor.)
  // 4. Return session.
  return MakeGarbageCollected<MediaKeySession>(script_state, this,
                                               session_type);
}

ScriptPromise MediaKeys::setServerCertificate(
    ScriptState* script_state,
    const DOMArrayPiece& server_certificate,
    ExceptionState& exception_state) {
  // From https://w3c.github.io/encrypted-media/#setServerCertificate
  // The setServerCertificate(serverCertificate) method provides a server
  // certificate to be used to encrypt messages to the license server.
  // It must run the following steps:
  // 1. If the Key System implementation represented by this object's cdm
  //    implementation value does not support server certificates, return
  //    a promise resolved with false.
  // TODO(jrummell): Provide a way to determine if the CDM supports this.
  // http://crbug.com/647816.
  //
  // 2. If serverCertificate is an empty array, return a promise rejected
  //    with a new a newly created TypeError.
  if (!server_certificate.ByteLengthAsSizeT()) {
    exception_state.ThrowTypeError("The serverCertificate parameter is empty.");
    return ScriptPromise();
  }

  // 3. Let certificate be a copy of the contents of the serverCertificate
  //    parameter.
  DOMArrayBuffer* server_certificate_buffer = DOMArrayBuffer::Create(
      server_certificate.Data(), server_certificate.ByteLengthAsSizeT());

  // 4. Let promise be a new promise.
  SetCertificateResultPromise* result =
      MakeGarbageCollected<SetCertificateResultPromise>(script_state, this);
  ScriptPromise promise = result->Promise();

  // 5. Run the following steps asynchronously. See SetServerCertificateTask().
  pending_actions_.push_back(PendingAction::CreatePendingSetServerCertificate(
      result, server_certificate_buffer));
  if (!timer_.IsActive())
    timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 6. Return promise.
  return promise;
}

void MediaKeys::SetServerCertificateTask(
    DOMArrayBuffer* server_certificate,
    ContentDecryptionModuleResult* result) {
  DVLOG(MEDIA_KEYS_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 5.1 Let cdm be the cdm during the initialization of this object.
  WebContentDecryptionModule* cdm = ContentDecryptionModule();

  // 5.2 Use the cdm to process certificate.
  cdm->SetServerCertificate(
      static_cast<unsigned char*>(server_certificate->Data()),
      server_certificate->ByteLengthAsSizeT(), result->Result());

  // 5.3 If any of the preceding steps failed, reject promise with a
  //     new DOMException whose name is the appropriate error name.
  // 5.4 Resolve promise.
  // (These are handled by Chromium and the CDM.)
}

ScriptPromise MediaKeys::getStatusForPolicy(
    ScriptState* script_state,
    const MediaKeysPolicy* media_keys_policy) {
  // TODO(xhwang): Pass MediaKeysPolicy classes all the way to Chromium when
  // we have more than one policy to check.
  String min_hdcp_version = media_keys_policy->minHdcpVersion();

  // Let promise be a new promise.
  GetStatusForPolicyResultPromise* result =
      MakeGarbageCollected<GetStatusForPolicyResultPromise>(script_state, this);
  ScriptPromise promise = result->Promise();

  // Run the following steps asynchronously. See GetStatusForPolicyTask().
  pending_actions_.push_back(
      PendingAction::CreatePendingGetStatusForPolicy(result, min_hdcp_version));
  if (!timer_.IsActive())
    timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // Return promise.
  return promise;
}

void MediaKeys::GetStatusForPolicyTask(const String& min_hdcp_version,
                                       ContentDecryptionModuleResult* result) {
  DVLOG(MEDIA_KEYS_LOG_LEVEL) << __func__ << ": " << min_hdcp_version;

  WebContentDecryptionModule* cdm = ContentDecryptionModule();
  cdm->GetStatusForPolicy(min_hdcp_version, result->Result());
}

bool MediaKeys::ReserveForMediaElement(HTMLMediaElement* media_element) {
  // If some other HtmlMediaElement already has a reference to us, fail.
  if (media_element_)
    return false;

  media_element_ = media_element;
  reserved_for_media_element_ = true;
  return true;
}

void MediaKeys::AcceptReservation() {
  reserved_for_media_element_ = false;
}

void MediaKeys::CancelReservation() {
  reserved_for_media_element_ = false;
  media_element_.Clear();
}

void MediaKeys::ClearMediaElement() {
  DCHECK(media_element_);
  media_element_.Clear();
}

bool MediaKeys::SessionTypeSupported(
    WebEncryptedMediaSessionType session_type) {
  for (size_t i = 0; i < supported_session_types_.size(); i++) {
    if (supported_session_types_[i] == session_type)
      return true;
  }

  return false;
}

void MediaKeys::TimerFired(TimerBase*) {
  DCHECK(pending_actions_.size());

  // Swap the queue to a local copy to avoid problems if resolving promises
  // run synchronously.
  HeapDeque<Member<PendingAction>> pending_actions;
  pending_actions.Swap(pending_actions_);

  while (!pending_actions.IsEmpty()) {
    PendingAction* action = pending_actions.TakeFirst();

    switch (action->GetType()) {
      case PendingAction::Type::kSetServerCertificate:
        SetServerCertificateTask(action->Data(), action->Result());
        break;

      case PendingAction::Type::kGetStatusForPolicy:
        GetStatusForPolicyTask(action->StringData(), action->Result());
        break;

      default:
        NOTREACHED();
    }
  }
}

WebContentDecryptionModule* MediaKeys::ContentDecryptionModule() {
  return cdm_.get();
}

void MediaKeys::Trace(Visitor* visitor) {
  visitor->Trace(pending_actions_);
  visitor->Trace(media_element_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void MediaKeys::ContextDestroyed() {
  timer_.Stop();
  pending_actions_.clear();

  // We don't need the CDM anymore. Only destroyed after all related
  // ExecutionContextLifecycleObservers have been stopped.
  cdm_.reset();
}

bool MediaKeys::HasPendingActivity() const {
  // Remain around if there are pending events.
  DVLOG(MEDIA_KEYS_LOG_LEVEL)
      << __func__ << "(" << this << ")"
      << (!pending_actions_.IsEmpty() ? " !pending_actions_.isEmpty()" : "")
      << (reserved_for_media_element_ ? " reserved_for_media_element_" : "");

  return !pending_actions_.IsEmpty() || reserved_for_media_element_;
}

}  // namespace blink
