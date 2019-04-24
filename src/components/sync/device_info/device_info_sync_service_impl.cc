// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_sync_service_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_tracker.h"
#include "components/sync/device_info/local_device_info_provider_impl.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace syncer {

DeviceInfoSyncServiceImpl::DeviceInfoSyncServiceImpl(
    OnceModelTypeStoreFactory model_type_store_factory,
    std::unique_ptr<LocalDeviceInfoProviderImpl> local_device_info_provider)
    : local_device_info_provider_(std::move(local_device_info_provider)),
      bridge_(local_device_info_provider_.get(),
              std::move(model_type_store_factory),
              std::make_unique<ClientTagBasedModelTypeProcessor>(
                  DEVICE_INFO,
                  /*dump_stack=*/base::BindRepeating(
                      &ReportUnrecoverableError,
                      local_device_info_provider_->GetChannel()))) {
  DCHECK(local_device_info_provider_);
}

DeviceInfoSyncServiceImpl::~DeviceInfoSyncServiceImpl() {}

LocalDeviceInfoProvider*
DeviceInfoSyncServiceImpl::GetLocalDeviceInfoProvider() {
  return local_device_info_provider_.get();
}

DeviceInfoTracker* DeviceInfoSyncServiceImpl::GetDeviceInfoTracker() {
  return &bridge_;
}

base::WeakPtr<ModelTypeControllerDelegate>
DeviceInfoSyncServiceImpl::GetControllerDelegate() {
  return bridge_.change_processor()->GetControllerDelegate();
}

void DeviceInfoSyncServiceImpl::InitLocalCacheGuid(
    const std::string& cache_guid,
    const std::string& session_name) {
  local_device_info_provider_->Initialize(cache_guid, session_name);
}

void DeviceInfoSyncServiceImpl::ClearLocalCacheGuid() {
  local_device_info_provider_->Clear();
}

}  // namespace syncer
