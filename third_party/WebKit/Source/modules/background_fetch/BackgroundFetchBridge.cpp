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
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

// static
BackgroundFetchBridge* BackgroundFetchBridge::From(
    ServiceWorkerRegistration* service_worker_registration) {
  DCHECK(service_worker_registration);

  BackgroundFetchBridge* bridge = static_cast<BackgroundFetchBridge*>(
      Supplement<ServiceWorkerRegistration>::From(service_worker_registration,
                                                  SupplementName()));

  if (!bridge) {
    bridge = new BackgroundFetchBridge(*service_worker_registration);
    Supplement<ServiceWorkerRegistration>::ProvideTo(
        *service_worker_registration, SupplementName(), bridge);
  }

  return bridge;
}

// static
const char* BackgroundFetchBridge::SupplementName() {
  return "BackgroundFetchBridge";
}

BackgroundFetchBridge::BackgroundFetchBridge(
    ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration) {}

BackgroundFetchBridge::~BackgroundFetchBridge() = default;

void BackgroundFetchBridge::Fetch(const String& id,
                                  Vector<WebServiceWorkerRequest> requests,
                                  const BackgroundFetchOptions& options,
                                  RegistrationCallback callback) {
  GetService()->Fetch(
      GetSupplementable()->WebRegistration()->RegistrationId(),
      GetSecurityOrigin(), id, std::move(requests),
      mojom::blink::BackgroundFetchOptions::From(options),
      ConvertToBaseCallback(
          WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                    WrapPersistent(this), WTF::Passed(std::move(callback)))));
}

void BackgroundFetchBridge::Abort(const String& id, AbortCallback callback) {
  GetService()->Abort(GetSupplementable()->WebRegistration()->RegistrationId(),
                      GetSecurityOrigin(), id,
                      ConvertToBaseCallback(std::move(callback)));
}

void BackgroundFetchBridge::UpdateUI(const String& id,
                                     const String& title,
                                     UpdateUICallback callback) {
  GetService()->UpdateUI(
      GetSupplementable()->WebRegistration()->RegistrationId(),
      GetSecurityOrigin(), id, title,
      ConvertToBaseCallback(std::move(callback)));
}

void BackgroundFetchBridge::GetRegistration(const String& id,
                                            RegistrationCallback callback) {
  GetService()->GetRegistration(
      GetSupplementable()->WebRegistration()->RegistrationId(),
      GetSecurityOrigin(), id,
      ConvertToBaseCallback(
          WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                    WrapPersistent(this), WTF::Passed(std::move(callback)))));
}

void BackgroundFetchBridge::DidGetRegistration(
    RegistrationCallback callback,
    mojom::blink::BackgroundFetchError error,
    mojom::blink::BackgroundFetchRegistrationPtr registration_ptr) {
  BackgroundFetchRegistration* registration =
      registration_ptr.To<BackgroundFetchRegistration*>();

  if (registration) {
    DCHECK_EQ(error, mojom::blink::BackgroundFetchError::NONE);
    registration->SetServiceWorkerRegistration(GetSupplementable());
  }

  callback(error, registration);
}

void BackgroundFetchBridge::GetIds(GetIdsCallback callback) {
  GetService()->GetIds(GetSupplementable()->WebRegistration()->RegistrationId(),
                       GetSecurityOrigin(),
                       ConvertToBaseCallback(std::move(callback)));
}

SecurityOrigin* BackgroundFetchBridge::GetSecurityOrigin() {
  return GetSupplementable()->GetExecutionContext()->GetSecurityOrigin();
}

mojom::blink::BackgroundFetchServicePtr& BackgroundFetchBridge::GetService() {
  if (!background_fetch_service_) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&background_fetch_service_));
  }
  return background_fetch_service_;
}

}  // namespace blink
