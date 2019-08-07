// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_sync_service_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_bridge.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace syncer {

DeviceInfoSyncServiceImpl::DeviceInfoSyncServiceImpl(
    OnceModelTypeStoreFactory model_type_store_factory,
    std::unique_ptr<MutableLocalDeviceInfoProvider>
        local_device_info_provider) {
  DCHECK(local_device_info_provider);

  // Make a copy of the channel to avoid relying on argument evaluation order.
  const version_info::Channel channel =
      local_device_info_provider->GetChannel();

  bridge_ = std::make_unique<DeviceInfoSyncBridge>(
      std::move(local_device_info_provider),
      std::move(model_type_store_factory),
      std::make_unique<ClientTagBasedModelTypeProcessor>(
          DEVICE_INFO,
          /*dump_stack=*/base::BindRepeating(&ReportUnrecoverableError,
                                             channel)));
}

DeviceInfoSyncServiceImpl::~DeviceInfoSyncServiceImpl() {}

LocalDeviceInfoProvider*
DeviceInfoSyncServiceImpl::GetLocalDeviceInfoProvider() {
  return bridge_->GetLocalDeviceInfoProvider();
}

DeviceInfoTracker* DeviceInfoSyncServiceImpl::GetDeviceInfoTracker() {
  return bridge_.get();
}

base::WeakPtr<ModelTypeControllerDelegate>
DeviceInfoSyncServiceImpl::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace syncer
