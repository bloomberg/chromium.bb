// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

#include "base/bind.h"
#include "base/time/default_clock.h"
#include "components/send_tab_to_self/send_tab_to_self_bridge.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace send_tab_to_self {

SendTabToSelfSyncService::SendTabToSelfSyncService() = default;

SendTabToSelfSyncService::SendTabToSelfSyncService(
    version_info::Channel channel,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::OnceModelTypeStoreFactory create_store_callback) {
  bridge_ = std::make_unique<send_tab_to_self::SendTabToSelfBridge>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SEND_TAB_TO_SELF,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      local_device_info_provider, base::DefaultClock::GetInstance(),
      std::move(create_store_callback));
}

SendTabToSelfSyncService::~SendTabToSelfSyncService() = default;

SendTabToSelfModel* SendTabToSelfSyncService::GetSendTabToSelfModel() {
  return bridge_.get();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
SendTabToSelfSyncService::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace send_tab_to_self
