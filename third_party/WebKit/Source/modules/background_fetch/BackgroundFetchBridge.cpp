// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchBridge.h"

#include <utility>
#include "modules/background_fetch/BackgroundFetchOptions.h"
#include "modules/background_fetch/BackgroundFetchRegistration.h"
#include "modules/background_fetch/BackgroundFetchTypeConverters.h"
#include "modules/background_fetch/IconDefinition.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {

// static
BackgroundFetchBridge* BackgroundFetchBridge::from(
    ServiceWorkerRegistration* serviceWorkerRegistration) {
  DCHECK(serviceWorkerRegistration);

  BackgroundFetchBridge* bridge = static_cast<BackgroundFetchBridge*>(
      Supplement<ServiceWorkerRegistration>::from(serviceWorkerRegistration,
                                                  supplementName()));

  if (!bridge) {
    bridge = new BackgroundFetchBridge(*serviceWorkerRegistration);
    Supplement<ServiceWorkerRegistration>::provideTo(*serviceWorkerRegistration,
                                                     supplementName(), bridge);
  }

  return bridge;
}

// static
const char* BackgroundFetchBridge::supplementName() {
  return "BackgroundFetchBridge";
}

BackgroundFetchBridge::BackgroundFetchBridge(
    ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration) {}

BackgroundFetchBridge::~BackgroundFetchBridge() = default;

void BackgroundFetchBridge::fetch(
    const String& tag,
    const BackgroundFetchOptions& options,
    std::unique_ptr<RegistrationCallback> callback) {
  getService()->Fetch(
      supplementable()->webRegistration()->registrationId(), tag,
      mojom::blink::BackgroundFetchOptions::From(options),
      convertToBaseCallback(
          WTF::bind(&BackgroundFetchBridge::didGetRegistration,
                    wrapPersistent(this), WTF::passed(std::move(callback)))));
}

void BackgroundFetchBridge::abort(const String& tag,
                                  std::unique_ptr<AbortCallback> callback) {
  getService()->Abort(supplementable()->webRegistration()->registrationId(),
                      tag, convertToBaseCallback(std::move(callback)));
}

void BackgroundFetchBridge::updateUI(
    const String& tag,
    const String& title,
    std::unique_ptr<UpdateUICallback> callback) {
  getService()->UpdateUI(supplementable()->webRegistration()->registrationId(),
                         tag, title,
                         convertToBaseCallback(std::move(callback)));
}

void BackgroundFetchBridge::getRegistration(
    const String& tag,
    std::unique_ptr<RegistrationCallback> callback) {
  getService()->GetRegistration(
      supplementable()->webRegistration()->registrationId(), tag,
      convertToBaseCallback(
          WTF::bind(&BackgroundFetchBridge::didGetRegistration,
                    wrapPersistent(this), WTF::passed(std::move(callback)))));
}

void BackgroundFetchBridge::didGetRegistration(
    std::unique_ptr<RegistrationCallback> callback,
    mojom::blink::BackgroundFetchError error,
    mojom::blink::BackgroundFetchRegistrationPtr registrationPtr) {
  BackgroundFetchRegistration* registration =
      registrationPtr.To<BackgroundFetchRegistration*>();

  if (registration) {
    DCHECK_EQ(error, mojom::blink::BackgroundFetchError::NONE);
    registration->setServiceWorkerRegistration(supplementable());
  }

  (*callback)(error, registration);
}

void BackgroundFetchBridge::getTags(std::unique_ptr<GetTagsCallback> callback) {
  getService()->GetTags(supplementable()->webRegistration()->registrationId(),
                        convertToBaseCallback(std::move(callback)));
}

mojom::blink::BackgroundFetchServicePtr& BackgroundFetchBridge::getService() {
  if (!m_backgroundFetchService) {
    Platform::current()->interfaceProvider()->getInterface(
        mojo::MakeRequest(&m_backgroundFetchService));
  }
  return m_backgroundFetchService;
}

}  // namespace blink
