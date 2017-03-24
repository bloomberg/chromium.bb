// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchRegistration.h"

#include "bindings/core/v8/ScriptState.h"
#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/IconDefinition.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

BackgroundFetchRegistration::BackgroundFetchRegistration(
    String tag,
    HeapVector<IconDefinition> icons,
    long long totalDownloadSize,
    String title)
    : m_tag(tag),
      m_icons(icons),
      m_totalDownloadSize(totalDownloadSize),
      m_title(title) {}

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

void BackgroundFetchRegistration::setServiceWorkerRegistration(
    ServiceWorkerRegistration* registration) {
  m_registration = registration;
}

String BackgroundFetchRegistration::tag() const {
  return m_tag;
}

HeapVector<IconDefinition> BackgroundFetchRegistration::icons() const {
  return m_icons;
}

long long BackgroundFetchRegistration::totalDownloadSize() const {
  return m_totalDownloadSize;
}

String BackgroundFetchRegistration::title() const {
  return m_title;
}

ScriptPromise BackgroundFetchRegistration::abort(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  DCHECK(m_registration);
  BackgroundFetchBridge::from(m_registration)
      ->abort(m_tag, WTF::bind(&BackgroundFetchRegistration::didAbort,
                               wrapPersistent(this), wrapPersistent(resolver)));

  return promise;
}

void BackgroundFetchRegistration::didAbort(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
      resolver->resolve(true /* success */);
      return;
    case mojom::blink::BackgroundFetchError::INVALID_TAG:
      resolver->resolve(false /* success */);
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_TAG:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

DEFINE_TRACE(BackgroundFetchRegistration) {
  visitor->trace(m_registration);
  visitor->trace(m_icons);
}

}  // namespace blink
