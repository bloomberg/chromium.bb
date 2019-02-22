// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/update_registration_ui_task.h"

#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

namespace background_fetch {

UpdateRegistrationUITask::UpdateRegistrationUITask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    UpdateRegistrationUICallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      title_(title),
      icon_(icon),
      callback_(std::move(callback)),
      weak_factory_(this) {
  DCHECK(title_ || icon_);
}

UpdateRegistrationUITask::~UpdateRegistrationUITask() = default;

void UpdateRegistrationUITask::Start() {
  if (title_ && icon_ && ShouldPersistIcon(*icon_)) {
    // Directly overwrite whatever's stored in the SWDB.
    SerializeIcon(*icon_,
                  base::BindOnce(&UpdateRegistrationUITask::DidSerializeIcon,
                                 weak_factory_.GetWeakPtr()));
    return;
  }

  service_worker_context()->GetRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {UIOptionsKey(registration_id_.unique_id())},
      base::BindOnce(&UpdateRegistrationUITask::DidGetUIOptions,
                     weak_factory_.GetWeakPtr()));
}

void UpdateRegistrationUITask::DidGetUIOptions(
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
    case DatabaseStatus::kNotFound:
    case DatabaseStatus::kOk:
      break;
  }

  if (data.empty() || !ui_options_.ParseFromString(data[0])) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  if (icon_ && ShouldPersistIcon(*icon_)) {
    ui_options_.clear_icon();
    SerializeIcon(*icon_,
                  base::BindOnce(&UpdateRegistrationUITask::DidSerializeIcon,
                                 weak_factory_.GetWeakPtr()));
  } else {
    StoreUIOptions();
  }
}

void UpdateRegistrationUITask::DidSerializeIcon(std::string serialized_icon) {
  ui_options_.set_icon(std::move(serialized_icon));
  StoreUIOptions();
}

void UpdateRegistrationUITask::StoreUIOptions() {
  if (title_)
    ui_options_.set_title(*title_);

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{UIOptionsKey(registration_id_.unique_id()),
        ui_options_.SerializeAsString()}},
      base::BindOnce(&UpdateRegistrationUITask::DidUpdateUIOptions,
                     weak_factory_.GetWeakPtr()));
}

void UpdateRegistrationUITask::DidUpdateUIOptions(
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void UpdateRegistrationUITask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  for (auto& observer : data_manager()->observers())
    observer.OnUpdatedUI(registration_id_, title_, icon_);

  ReportStorageError();

  std::move(callback_).Run(error);
  Finished();  // Destroys |this|.
}

std::string UpdateRegistrationUITask::HistogramName() const {
  return "UpdateRegistrationUITask";
}

}  // namespace background_fetch

}  // namespace content
