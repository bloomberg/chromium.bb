// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchRegistration.h"

#include "core/dom/DOMException.h"
#include "core/dom/events/Event.h"
#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/IconDefinition.h"
#include "modules/event_target_modules_names.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

BackgroundFetchRegistration::BackgroundFetchRegistration(
    const String& developer_id,
    const String& unique_id,
    unsigned long long upload_total,
    unsigned long long uploaded,
    unsigned long long download_total,
    unsigned long long downloaded,
    HeapVector<IconDefinition> icons,
    const String& title)
    : developer_id_(developer_id),
      unique_id_(unique_id),
      upload_total_(upload_total),
      uploaded_(uploaded),
      download_total_(download_total),
      downloaded_(downloaded),
      icons_(icons),
      title_(title),
      observer_binding_(this) {}

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

void BackgroundFetchRegistration::Initialize(
    ServiceWorkerRegistration* registration) {
  DCHECK(!registration_);
  DCHECK(registration);

  registration_ = registration;

  mojom::blink::BackgroundFetchRegistrationObserverPtr observer;
  observer_binding_.Bind(mojo::MakeRequest(&observer));

  BackgroundFetchBridge::From(registration_)
      ->AddRegistrationObserver(unique_id_, std::move(observer));
}

void BackgroundFetchRegistration::OnProgress(uint64_t upload_total,
                                             uint64_t uploaded,
                                             uint64_t download_total,
                                             uint64_t downloaded) {
  upload_total_ = upload_total;
  uploaded_ = uploaded;
  download_total_ = download_total;
  downloaded_ = downloaded;

  ExecutionContext* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed())
    return;

  DCHECK(context->IsContextThread());
  DispatchEvent(Event::Create(EventTypeNames::progress));
}

String BackgroundFetchRegistration::id() const {
  return developer_id_;
}

unsigned long long BackgroundFetchRegistration::uploadTotal() const {
  return upload_total_;
}

unsigned long long BackgroundFetchRegistration::uploaded() const {
  return uploaded_;
}

unsigned long long BackgroundFetchRegistration::downloadTotal() const {
  return download_total_;
}

unsigned long long BackgroundFetchRegistration::downloaded() const {
  return downloaded_;
}

HeapVector<IconDefinition> BackgroundFetchRegistration::icons() const {
  return icons_;
}

String BackgroundFetchRegistration::title() const {
  return title_;
}

const AtomicString& BackgroundFetchRegistration::InterfaceName() const {
  return EventTargetNames::BackgroundFetchRegistration;
}

ExecutionContext* BackgroundFetchRegistration::GetExecutionContext() const {
  DCHECK(registration_);
  return registration_->GetExecutionContext();
}

ScriptPromise BackgroundFetchRegistration::abort(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  DCHECK(registration_);
  BackgroundFetchBridge::From(registration_)
      ->Abort(developer_id_, unique_id_,
              WTF::Bind(&BackgroundFetchRegistration::DidAbort,
                        WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BackgroundFetchRegistration::DidAbort(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      resolver->Resolve(true /* success */);
      return;
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      resolver->Resolve(false /* success */);
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      resolver->Reject(DOMException::Create(
          kAbortError, "Failed to abort registration due to I/O error."));
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

void BackgroundFetchRegistration::Dispose() {
  observer_binding_.Close();
}

void BackgroundFetchRegistration::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(icons_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
