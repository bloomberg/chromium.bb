// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchBridge.h"

#include <utility>
#include "modules/background_fetch/BackgroundFetchOptions.h"
#include "modules/background_fetch/BackgroundFetchRegistration.h"
#include "modules/background_fetch/BackgroundFetchTypeConverters.h"
#include "modules/background_fetch/IconDefinition.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "services/service_manager/public/cpp/interface_provider.h"

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

void BackgroundFetchBridge::Fetch(const String& developer_id,
                                  Vector<WebServiceWorkerRequest> requests,
                                  const BackgroundFetchOptions& options,
                                  RegistrationCallback callback) {
  GetService()->Fetch(
      GetSupplementable()->WebRegistration()->RegistrationId(), developer_id,
      std::move(requests), mojom::blink::BackgroundFetchOptions::From(options),
      WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                WrapPersistent(this), WTF::Passed(std::move(callback))));
}

void BackgroundFetchBridge::Abort(const String& developer_id,
                                  const String& unique_id,
                                  AbortCallback callback) {
  GetService()->Abort(GetSupplementable()->WebRegistration()->RegistrationId(),
                      developer_id, unique_id, std::move(callback));
}

void BackgroundFetchBridge::UpdateUI(const String& developer_id,
                                     const String& unique_id,
                                     const String& title,
                                     UpdateUICallback callback) {
  GetService()->UpdateUI(unique_id, title, std::move(callback));
}

void BackgroundFetchBridge::GetRegistration(const String& developer_id,
                                            RegistrationCallback callback) {
  GetService()->GetRegistration(
      GetSupplementable()->WebRegistration()->RegistrationId(), developer_id,
      WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                WrapPersistent(this), WTF::Passed(std::move(callback))));
}

void BackgroundFetchBridge::DidGetRegistration(
    RegistrationCallback callback,
    mojom::blink::BackgroundFetchError error,
    mojom::blink::BackgroundFetchRegistrationPtr registration_ptr) {
  BackgroundFetchRegistration* registration =
      registration_ptr.To<BackgroundFetchRegistration*>();

  if (registration) {
    DCHECK_EQ(error, mojom::blink::BackgroundFetchError::NONE);
    registration->Initialize(GetSupplementable());
  }

  std::move(callback).Run(error, registration);
}

void BackgroundFetchBridge::GetDeveloperIds(GetDeveloperIdsCallback callback) {
  GetService()->GetDeveloperIds(
      GetSupplementable()->WebRegistration()->RegistrationId(),
      std::move(callback));
}

void BackgroundFetchBridge::AddRegistrationObserver(
    const String& unique_id,
    mojom::blink::BackgroundFetchRegistrationObserverPtr observer) {
  GetService()->AddRegistrationObserver(unique_id, std::move(observer));
}

mojom::blink::BackgroundFetchService* BackgroundFetchBridge::GetService() {
  if (!background_fetch_service_) {
    auto request = mojo::MakeRequest(&background_fetch_service_);
    if (auto* interface_provider = GetSupplementable()
                                       ->GetExecutionContext()
                                       ->GetInterfaceProvider()) {
      interface_provider->GetInterface(std::move(request));
    }
  }
  return background_fetch_service_.get();
}

}  // namespace blink
