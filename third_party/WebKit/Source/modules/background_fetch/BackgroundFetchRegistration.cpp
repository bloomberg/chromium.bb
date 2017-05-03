// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchRegistration.h"

#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/IconDefinition.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

BackgroundFetchRegistration::BackgroundFetchRegistration(
    String tag,
    HeapVector<IconDefinition> icons,
    long long total_download_size,
    String title)
    : tag_(tag),
      icons_(icons),
      total_download_size_(total_download_size),
      title_(title) {}

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

void BackgroundFetchRegistration::SetServiceWorkerRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  registration_ = registration;
}

String BackgroundFetchRegistration::tag() const {
  return tag_;
}

HeapVector<IconDefinition> BackgroundFetchRegistration::icons() const {
  return icons_;
}

long long BackgroundFetchRegistration::totalDownloadSize() const {
  return total_download_size_;
}

String BackgroundFetchRegistration::title() const {
  return title_;
}

ScriptPromise BackgroundFetchRegistration::abort(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  DCHECK(registration_);
  BackgroundFetchBridge::From(registration_)
      ->Abort(tag_, WTF::Bind(&BackgroundFetchRegistration::DidAbort,
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
    case mojom::blink::BackgroundFetchError::INVALID_TAG:
      resolver->Resolve(false /* success */);
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_TAG:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

DEFINE_TRACE(BackgroundFetchRegistration) {
  visitor->Trace(registration_);
  visitor->Trace(icons_);
}

}  // namespace blink
