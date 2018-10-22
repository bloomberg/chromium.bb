// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_core.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/prefs/pref_service.h"

namespace policy {

CloudPolicyCore::Observer::~Observer() {}

void CloudPolicyCore::Observer::OnRemoteCommandsServiceStarted(
    CloudPolicyCore* core) {
}

CloudPolicyCore::CloudPolicyCore(
    const std::string& policy_type,
    const std::string& settings_entity_id,
    CloudPolicyStore* store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : policy_type_(policy_type),
      settings_entity_id_(settings_entity_id),
      store_(store),
      task_runner_(task_runner) {}

CloudPolicyCore::~CloudPolicyCore() {}

void CloudPolicyCore::Connect(std::unique_ptr<CloudPolicyClient> client) {
  CHECK(!client_);
  CHECK(client);
  client_ = std::move(client);
  service_.reset(new CloudPolicyService(policy_type_, settings_entity_id_,
                                        client_.get(), store_));
  for (auto& observer : observers_)
    observer.OnCoreConnected(this);
}

void CloudPolicyCore::Disconnect() {
  if (client_)
    for (auto& observer : observers_)
      observer.OnCoreDisconnecting(this);
  refresh_delay_.reset();
  refresh_scheduler_.reset();
  remote_commands_service_.reset();
  service_.reset();
  client_.reset();
}

void CloudPolicyCore::StartRemoteCommandsService(
    std::unique_ptr<RemoteCommandsFactory> factory) {
  DCHECK(client_);
  DCHECK(factory);

  remote_commands_service_.reset(
      new RemoteCommandsService(std::move(factory), client_.get()));

  // Do an initial remote commands fetch immediately.
  remote_commands_service_->FetchRemoteCommands();

  for (auto& observer : observers_)
    observer.OnRemoteCommandsServiceStarted(this);
}

void CloudPolicyCore::RefreshSoon() {
  if (refresh_scheduler_)
    refresh_scheduler_->RefreshSoon();
}

void CloudPolicyCore::StartRefreshScheduler() {
  if (!refresh_scheduler_) {
    refresh_scheduler_.reset(
        new CloudPolicyRefreshScheduler(client_.get(), store_, task_runner_));
    UpdateRefreshDelayFromPref();
    for (auto& observer : observers_)
      observer.OnRefreshSchedulerStarted(this);
  }
}

void CloudPolicyCore::TrackRefreshDelayPref(
    PrefService* pref_service,
    const std::string& refresh_pref_name) {
  refresh_delay_.reset(new IntegerPrefMember());
  refresh_delay_->Init(refresh_pref_name, pref_service,
                       base::Bind(&CloudPolicyCore::UpdateRefreshDelayFromPref,
                                  base::Unretained(this)));
  UpdateRefreshDelayFromPref();
}

void CloudPolicyCore::AddObserver(CloudPolicyCore::Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyCore::RemoveObserver(CloudPolicyCore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyCore::UpdateRefreshDelayFromPref() {
  if (refresh_scheduler_ && refresh_delay_)
    refresh_scheduler_->SetDesiredRefreshDelay(refresh_delay_->GetValue());
}

}  // namespace policy
