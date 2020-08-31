// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/proxy_tabs_data_type_controller.h"

#include <utility>

#include "components/sync/driver/configure_context.h"
#include "components/sync/engine/model_type_configurer.h"

namespace sync_sessions {

ProxyTabsDataTypeController::ProxyTabsDataTypeController(
    const base::RepeatingCallback<void(State)>& state_changed_cb)
    : DataTypeController(syncer::PROXY_TABS),
      state_changed_cb_(state_changed_cb),
      state_(NOT_RUNNING) {}

ProxyTabsDataTypeController::~ProxyTabsDataTypeController() {}

void ProxyTabsDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(configure_context.sync_mode, syncer::SyncMode::kFull);
  state_ = MODEL_LOADED;
  state_changed_cb_.Run(state_);
  model_load_callback.Run(type(), syncer::SyncError());
}

syncer::DataTypeController::RegisterWithBackendResult
ProxyTabsDataTypeController::RegisterWithBackend(
    syncer::ModelTypeConfigurer* configurer) {
  DCHECK(configurer);
  DCHECK_EQ(MODEL_LOADED, state_);

  // Proxy type doesn't need to be registered with ModelTypeRegistry as it
  // doesn't need update handler, client doesn't expect updates of this type
  // from the server. We still need to register proxy type because
  // AddClientConfigParamsToMessage decides the value of tabs_datatype_enabled
  // based on presence of proxy types in the set of enabled types.
  configurer->ActivateProxyDataType(type());

  state_ = RUNNING;
  state_changed_cb_.Run(state_);

  return REGISTRATION_IGNORED;
}

void ProxyTabsDataTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  state_ = NOT_RUNNING;
  state_changed_cb_.Run(state_);
  std::move(callback).Run();
}

syncer::DataTypeController::State ProxyTabsDataTypeController::state() const {
  return state_;
}

void ProxyTabsDataTypeController::DeactivateDataType(
    syncer::ModelTypeConfigurer* configurer) {
  if (state_ == RUNNING) {
    configurer->DeactivateProxyDataType(type());
    state_ = MODEL_LOADED;
  }
}

void ProxyTabsDataTypeController::GetAllNodes(AllNodesCallback callback) {
  std::move(callback).Run(type(), std::make_unique<base::ListValue>());
}

void ProxyTabsDataTypeController::GetStatusCounters(
    StatusCountersCallback callback) {
  syncer::StatusCounters counters;
  std::move(callback).Run(type(), counters);
}

void ProxyTabsDataTypeController::RecordMemoryUsageAndCountsHistograms() {}

}  // namespace sync_sessions
