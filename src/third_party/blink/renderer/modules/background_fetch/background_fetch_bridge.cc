// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"

#include <utility>
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_options.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_type_converters.h"
#include "third_party/blink/renderer/modules/manifest/image_resource.h"

namespace blink {

// static
BackgroundFetchBridge* BackgroundFetchBridge::From(
    ServiceWorkerRegistration* service_worker_registration) {
  DCHECK(service_worker_registration);

  BackgroundFetchBridge* bridge =
      Supplement<ServiceWorkerRegistration>::From<BackgroundFetchBridge>(
          service_worker_registration);

  if (!bridge) {
    bridge = MakeGarbageCollected<BackgroundFetchBridge>(
        *service_worker_registration);
    ProvideTo(*service_worker_registration, bridge);
  }

  return bridge;
}

// static
const char BackgroundFetchBridge::kSupplementName[] = "BackgroundFetchBridge";

BackgroundFetchBridge::BackgroundFetchBridge(
    ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration) {}

BackgroundFetchBridge::~BackgroundFetchBridge() = default;

void BackgroundFetchBridge::GetIconDisplaySize(
    GetIconDisplaySizeCallback callback) {
  GetService()->GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchBridge::MatchRequests(
    const String& developer_id,
    const String& unique_id,
    mojom::blink::FetchAPIRequestPtr request_to_match,
    mojom::blink::CacheQueryOptionsPtr cache_query_options,
    bool match_all,
    mojom::blink::BackgroundFetchService::MatchRequestsCallback callback) {
  GetService()->MatchRequests(
      GetSupplementable()->RegistrationId(), developer_id, unique_id,
      std::move(request_to_match), std::move(cache_query_options), match_all,
      std::move(callback));
}

void BackgroundFetchBridge::Fetch(
    const String& developer_id,
    Vector<mojom::blink::FetchAPIRequestPtr> requests,
    mojom::blink::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    mojom::blink::BackgroundFetchUkmDataPtr ukm_data,
    RegistrationCallback callback) {
  GetService()->Fetch(
      GetSupplementable()->RegistrationId(), developer_id, std::move(requests),
      std::move(options), icon, std::move(ukm_data),
      WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                WrapPersistent(this), WTF::Passed(std::move(callback))));
}

void BackgroundFetchBridge::Abort(const String& developer_id,
                                  const String& unique_id,
                                  AbortCallback callback) {
  GetService()->Abort(GetSupplementable()->RegistrationId(), developer_id,
                      unique_id, std::move(callback));
}

void BackgroundFetchBridge::UpdateUI(const String& developer_id,
                                     const String& unique_id,
                                     const String& title,
                                     const SkBitmap& icon,
                                     UpdateUICallback callback) {
  if (title.IsNull() && icon.isNull()) {
    std::move(callback).Run(
        mojom::blink::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  GetService()->UpdateUI(GetSupplementable()->RegistrationId(), developer_id,
                         unique_id, title, icon, std::move(callback));
}

void BackgroundFetchBridge::GetRegistration(const String& developer_id,
                                            RegistrationCallback callback) {
  GetService()->GetRegistration(
      GetSupplementable()->RegistrationId(), developer_id,
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
    DCHECK_EQ(registration->result(), "");
    registration->Initialize(GetSupplementable());
  }

  std::move(callback).Run(error, registration);
}

void BackgroundFetchBridge::GetDeveloperIds(GetDeveloperIdsCallback callback) {
  GetService()->GetDeveloperIds(GetSupplementable()->RegistrationId(),
                                std::move(callback));
}

void BackgroundFetchBridge::AddRegistrationObserver(
    const String& unique_id,
    mojom::blink::BackgroundFetchRegistrationObserverPtr observer) {
  GetService()->AddRegistrationObserver(unique_id, std::move(observer));
}

mojom::blink::BackgroundFetchService* BackgroundFetchBridge::GetService() {
  if (!background_fetch_service_) {
    auto request = mojo::MakeRequest(
        &background_fetch_service_,
        GetSupplementable()->GetExecutionContext()->GetTaskRunner(
            TaskType::kBackgroundFetch));
    if (auto* interface_provider = GetSupplementable()
                                       ->GetExecutionContext()
                                       ->GetInterfaceProvider()) {
      interface_provider->GetInterface(std::move(request));
    }
  }
  return background_fetch_service_.get();
}

}  // namespace blink
