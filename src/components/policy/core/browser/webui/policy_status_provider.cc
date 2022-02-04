// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_status_provider.h"

#include "base/time/time.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Formats the association state indicated by |data|. If |data| is NULL, the
// state is considered to be UNMANAGED.
std::u16string FormatAssociationState(const em::PolicyData* data) {
  if (data) {
    switch (data->state()) {
      case em::PolicyData::ACTIVE:
        return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_ACTIVE);
      case em::PolicyData::UNMANAGED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
      case em::PolicyData::DEPROVISIONED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_DEPROVISIONED);
    }
    NOTREACHED() << "Unknown state " << data->state();
  }

  // Default to UNMANAGED for the case of missing policy or bad state enum.
  return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
}

}  // namespace

PolicyStatusProvider::PolicyStatusProvider() = default;

PolicyStatusProvider::~PolicyStatusProvider() = default;

void PolicyStatusProvider::SetStatusChangeCallback(
    const base::RepeatingClosure& callback) {
  callback_ = callback;
}

// static
void PolicyStatusProvider::GetStatus(base::DictionaryValue* dict) {
  // This method is called when the client is not enrolled.
  // Thus leaving the dict without any changes.
}

void PolicyStatusProvider::NotifyStatusChange() {
  if (callback_)
    callback_.Run();
}

// static
void PolicyStatusProvider::GetStatusFromCore(const CloudPolicyCore* core,
                                             base::DictionaryValue* dict) {
  const CloudPolicyStore* store = core->store();
  const CloudPolicyClient* client = core->client();
  const CloudPolicyRefreshScheduler* refresh_scheduler =
      core->refresh_scheduler();

  const std::u16string status = GetPolicyStatusFromStore(store, client);

  const em::PolicyData* policy = store->policy();
  GetStatusFromPolicyData(policy, dict);

  base::TimeDelta refresh_interval = base::Milliseconds(
      refresh_scheduler ? refresh_scheduler->GetActualRefreshDelay()
                        : CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);

  // In case state_keys aren't available, we have no scheduler. See also
  // DeviceCloudPolicyInitializer::TryToCreateClient and b/181140445
  base::Time last_refresh_time =
      refresh_scheduler ? refresh_scheduler->last_refresh()
                        : policy && policy->has_timestamp()
                              ? base::Time::FromJavaTime(policy->timestamp())
                              : base::Time();

  bool no_error = store->status() == CloudPolicyStore::STATUS_OK && client &&
                  client->status() == DM_STATUS_SUCCESS;
  dict->SetBoolean("error", !no_error);
  dict->SetBoolean(
      "policiesPushAvailable",
      refresh_scheduler ? refresh_scheduler->invalidations_available() : false);
  dict->SetString("status", status);
  dict->SetString(
      "refreshInterval",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_SHORT, refresh_interval));
  dict->SetString("timeSinceLastRefresh",
                  GetTimeSinceLastRefreshString(last_refresh_time));
}

// static
void PolicyStatusProvider::GetStatusFromPolicyData(
    const em::PolicyData* policy,
    base::DictionaryValue* dict) {
  std::string client_id = policy ? policy->device_id() : std::string();
  std::string username = policy ? policy->username() : std::string();

  if (policy && policy->has_annotated_asset_id())
    dict->SetString("assetId", policy->annotated_asset_id());
  if (policy && policy->has_annotated_location())
    dict->SetString("location", policy->annotated_location());
  if (policy && policy->has_directory_api_id())
    dict->SetString("directoryApiId", policy->directory_api_id());
  if (policy && policy->has_gaia_id())
    dict->SetString("gaiaId", policy->gaia_id());

  dict->SetString("clientId", client_id);
  dict->SetString("username", username);
}

// CloudPolicyStore errors take precedence to show in the status message.
// Other errors (such as transient policy fetching problems) get displayed
// only if CloudPolicyStore is in STATUS_OK.
// static
std::u16string PolicyStatusProvider::GetPolicyStatusFromStore(
    const CloudPolicyStore* store,
    const CloudPolicyClient* client) {
  if (store->status() == CloudPolicyStore::STATUS_OK) {
    if (client && client->status() != DM_STATUS_SUCCESS)
      return FormatDeviceManagementStatus(client->status());
    else if (!store->is_managed())
      return FormatAssociationState(store->policy());
  }

  return FormatStoreStatus(store->status(), store->validation_status());
}

// static
std::u16string PolicyStatusProvider::GetTimeSinceLastRefreshString(
    base::Time last_refresh_time) {
  if (last_refresh_time.is_null())
    return l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED);
  base::Time now = base::Time::NowFromSystemTime();
  base::TimeDelta elapsed_time;
  if (now > last_refresh_time)
    elapsed_time = now - last_refresh_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

}  // namespace policy
