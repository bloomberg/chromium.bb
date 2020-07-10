// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_driver.h"

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

namespace {
const char kInstanceIDFieldTrialName[] = "InstanceID";
const char kInstanceIDFieldTrialDisabledGroupPrefix[] = "Disabled";
}  // namespace

// static
bool InstanceIDDriver::IsInstanceIDEnabled() {
  std::string group_name =
      base::FieldTrialList::FindFullName(kInstanceIDFieldTrialName);
  return !base::StartsWith(group_name, kInstanceIDFieldTrialDisabledGroupPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

InstanceIDDriver::InstanceIDDriver(gcm::GCMDriver* gcm_driver)
    : gcm_driver_(gcm_driver) {
}

InstanceIDDriver::~InstanceIDDriver() {
}

InstanceID* InstanceIDDriver::GetInstanceID(const std::string& app_id) {
  auto iter = instance_id_map_.find(app_id);
  if (iter != instance_id_map_.end())
    return iter->second.get();

  std::unique_ptr<InstanceID> instance_id =
      InstanceID::CreateInternal(app_id, gcm_driver_);
  InstanceID* instance_id_ptr = instance_id.get();
  instance_id_map_.insert(std::make_pair(app_id, std::move(instance_id)));
  return instance_id_ptr;
}

void InstanceIDDriver::RemoveInstanceID(const std::string& app_id) {
  instance_id_map_.erase(app_id);
}

bool InstanceIDDriver::ExistsInstanceID(const std::string& app_id) const {
  return instance_id_map_.find(app_id) != instance_id_map_.end();
}

}  // namespace instance_id
