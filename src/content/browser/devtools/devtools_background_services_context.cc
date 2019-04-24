// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_background_services_context.h"

#include <algorithm>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "url/origin.h"

namespace content {

namespace {

std::string CreateEntryKeyPrefix(devtools::proto::BackgroundService service) {
  DCHECK_NE(service, devtools::proto::BackgroundService::UNKNOWN);
  return "devtools_background_services_" + base::NumberToString(service) + "_";
}

std::string CreateEntryKey(devtools::proto::BackgroundService service) {
  return CreateEntryKeyPrefix(service) + base::GenerateGUID();
}

void DidLogServiceEvent(blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(rayankans): Log errors to UMA.
}

void DidClearServiceEvents(blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(rayankans): Log errors to UMA.
}

}  // namespace

DevToolsBackgroundServicesContext::DevToolsBackgroundServicesContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : browser_context_(browser_context),
      service_worker_context_(std::move(service_worker_context)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto expiration_times =
      GetContentClient()->browser()->GetDevToolsBackgroundServiceExpirations(
          browser_context_);

  for (const auto& expiration_time : expiration_times) {
    DCHECK(devtools::proto::BackgroundService_IsValid(expiration_time.first));
    expiration_times_[expiration_time.first] = expiration_time.second;
  }
}

DevToolsBackgroundServicesContext::~DevToolsBackgroundServicesContext() =
    default;

void DevToolsBackgroundServicesContext::StartRecording(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(!IsRecording(service));

  // TODO(rayankans): Make the time delay finch configurable.
  base::Time expiration_time = base::Time::Now() + base::TimeDelta::FromDays(3);
  expiration_times_[service] = expiration_time;

  GetContentClient()->browser()->UpdateDevToolsBackgroundServiceExpiration(
      browser_context_, service, expiration_time);
}

void DevToolsBackgroundServicesContext::StopRecording(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(IsRecording(service));
  expiration_times_[service] = base::Time();

  GetContentClient()->browser()->UpdateDevToolsBackgroundServiceExpiration(
      browser_context_, service, base::Time());
}

bool DevToolsBackgroundServicesContext::IsRecording(
    devtools::proto::BackgroundService service) {
  return base::Time::Now() < expiration_times_[service];
}

void DevToolsBackgroundServicesContext::GetLoggedBackgroundServiceEvents(
    devtools::proto::BackgroundService service,
    GetLoggedBackgroundServiceEventsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      CreateEntryKeyPrefix(service),
      base::BindOnce(&DevToolsBackgroundServicesContext::DidGetUserData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DevToolsBackgroundServicesContext::DidGetUserData(
    GetLoggedBackgroundServiceEventsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<devtools::proto::BackgroundServiceEvent> events;

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // TODO(rayankans): Log errors to UMA.
    std::move(callback).Run(events);
    return;
  }

  events.reserve(user_data.size());
  for (const auto& data : user_data) {
    devtools::proto::BackgroundServiceEvent event;
    if (!event.ParseFromString(data.second)) {
      // TODO(rayankans): Log errors to UMA.
      std::move(callback).Run({});
      return;
    }
    DCHECK_EQ(data.first, event.service_worker_registration_id());
    events.push_back(std::move(event));
  }

  std::sort(events.begin(), events.end(),
            [](const auto& state1, const auto& state2) {
              return state1.timestamp() < state2.timestamp();
            });
  std::move(callback).Run(std::move(events));
}

void DevToolsBackgroundServicesContext::ClearLoggedBackgroundServiceEvents(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->ClearUserDataForAllRegistrationsByKeyPrefix(
      CreateEntryKeyPrefix(service), base::BindOnce(&DidClearServiceEvents));
}

void DevToolsBackgroundServicesContext::LogBackgroundServiceEvent(
    uint64_t service_worker_registration_id,
    const url::Origin& origin,
    devtools::proto::BackgroundService service,
    const std::string& event_name,
    const std::string& instance_id,
    const std::map<std::string, std::string>& event_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsRecording(service))
    return;

  devtools::proto::BackgroundServiceEvent event;
  event.set_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  event.set_origin(origin.GetURL().spec());
  event.set_service_worker_registration_id(service_worker_registration_id);
  event.set_background_service(service);
  event.set_event_name(event_name);
  event.set_instance_id(instance_id);
  event.mutable_event_metadata()->insert(event_metadata.begin(),
                                         event_metadata.end());

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, origin.GetURL(),
      {{CreateEntryKey(event.background_service()), event.SerializeAsString()}},
      base::BindOnce(&DidLogServiceEvent));
}

}  // namespace content
