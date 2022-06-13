// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_load_manager.h"

#include <map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/sync/base/model_type.h"

namespace syncer {

ModelLoadManager::ModelLoadManager(
    const DataTypeController::TypeMap* controllers,
    ModelLoadManagerDelegate* processor)
    : controllers_(controllers), delegate_(processor) {}

ModelLoadManager::~ModelLoadManager() = default;

void ModelLoadManager::Initialize(ModelTypeSet desired_types,
                                  ModelTypeSet preferred_types,
                                  const ConfigureContext& context) {
  // |desired_types| must be a subset of |preferred_types|.
  DCHECK(preferred_types.HasAll(desired_types));

  bool sync_mode_changed = configure_context_.sync_mode != context.sync_mode;

  configure_context_ = context;

  // Only keep types that have controllers.
  desired_types_.Clear();
  for (ModelType type : desired_types) {
    auto dtc_iter = controllers_->find(type);
    if (dtc_iter != controllers_->end()) {
      const DataTypeController* dtc = dtc_iter->second.get();
      // Controllers in a FAILED state should have been filtered out by the
      // DataTypeManager.
      DCHECK_NE(dtc->state(), DataTypeController::FAILED);
      desired_types_.Put(type);
    }
  }

  DVLOG(1) << "ModelLoadManager: Initializing for "
           << ModelTypeSetToString(desired_types_);

  notified_about_ready_for_configure_ = false;

  DVLOG(1) << "ModelLoadManager: Stopping disabled types.";
  std::map<DataTypeController*, ShutdownReason> types_to_stop;
  for (const auto& type_and_dtc : *controllers_) {
    DataTypeController* dtc = type_and_dtc.second.get();
    // We generally stop all data types which are not desired. When the storage
    // option changes, we need to restart all data types so that they can
    // re-wire to the correct storage.
    bool should_stop = !desired_types_.Has(dtc->type()) || sync_mode_changed;
    // If the datatype is already STOPPING, we also wait for it to stop, to make
    // sure it's ready to start again (if appropriate).
    if ((should_stop && dtc->state() != DataTypeController::NOT_RUNNING) ||
        dtc->state() == DataTypeController::STOPPING) {
      // Note: STOP_SYNC means we'll keep the Sync data around; DISABLE_SYNC
      // means we'll clear it.
      ShutdownReason reason = preferred_types.Has(dtc->type())
                                  ? ShutdownReason::STOP_SYNC_AND_KEEP_DATA
                                  : ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA;
      // If we're switching to transport-only mode, don't clear any old data.
      // The reason is that if a user temporarily disables Sync, we don't want
      // to wipe (and later redownload) all their data, just because Sync
      // restarted in transport-only mode.
      if (sync_mode_changed &&
          configure_context_.sync_mode == SyncMode::kTransportOnly) {
        reason = ShutdownReason::STOP_SYNC_AND_KEEP_DATA;
      }
      types_to_stop[dtc] = reason;
    }
  }

  // Run LoadDesiredTypes() only after all relevant types are stopped.
  // TODO(mastiz): Add test coverage to this waiting logic, including the
  // case where the datatype is STOPPING when this function is called.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      types_to_stop.size(), base::BindOnce(&ModelLoadManager::LoadDesiredTypes,
                                           weak_ptr_factory_.GetWeakPtr()));

  for (const auto& dtc_and_reason : types_to_stop) {
    DataTypeController* dtc = dtc_and_reason.first;
    const ShutdownReason reason = dtc_and_reason.second;
    DVLOG(1) << "ModelLoadManager: stop " << dtc->name() << " due to "
             << ShutdownReasonToString(reason);
    StopDatatypeImpl(SyncError(), reason, dtc, barrier_closure);
  }
}

void ModelLoadManager::StopDatatype(ModelType type,
                                    ShutdownReason shutdown_reason,
                                    SyncError error) {
  DCHECK(error.IsSet());
  desired_types_.Remove(type);

  DataTypeController* dtc = controllers_->find(type)->second.get();
  if (dtc->state() != DataTypeController::NOT_RUNNING &&
      dtc->state() != DataTypeController::STOPPING) {
    StopDatatypeImpl(error, shutdown_reason, dtc, base::DoNothing());
  }

  // Removing a desired type may mean all models are now loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::StopDatatypeImpl(
    const SyncError& error,
    ShutdownReason shutdown_reason,
    DataTypeController* dtc,
    DataTypeController::StopCallback callback) {
  loaded_types_.Remove(dtc->type());

  DCHECK(error.IsSet() || (dtc->state() != DataTypeController::NOT_RUNNING));

  delegate_->OnSingleDataTypeWillStop(dtc->type(), error);

  // Note: Depending on |shutdown_reason|, USS types might clear their metadata
  // in response to Stop().
  dtc->Stop(shutdown_reason, std::move(callback));
}

void ModelLoadManager::LoadDesiredTypes() {
  // Note: |desired_types_| might be modified during iteration (e.g. in
  // ModelLoadCallback()), so make a copy.
  const ModelTypeSet types = desired_types_;
  for (ModelType type : types) {
    auto dtc_iter = controllers_->find(type);
    DCHECK(dtc_iter != controllers_->end());
    DataTypeController* dtc = dtc_iter->second.get();
    DCHECK_NE(DataTypeController::STOPPING, dtc->state());
    if (dtc->state() == DataTypeController::NOT_RUNNING) {
      DCHECK(!loaded_types_.Has(dtc->type()));
      dtc->LoadModels(configure_context_,
                      base::BindRepeating(&ModelLoadManager::ModelLoadCallback,
                                          weak_ptr_factory_.GetWeakPtr()));
    }
  }
  // It's possible that all models are already loaded.
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::Stop(ShutdownReason shutdown_reason) {
  // Ignore callbacks from controllers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop started data types.
  for (const auto& type_and_dtc : *controllers_) {
    DataTypeController* dtc = type_and_dtc.second.get();
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      // We don't really wait until all datatypes have been fully stopped, which
      // is only required (and in fact waited for) when Initialize() is called.
      StopDatatypeImpl(SyncError(), shutdown_reason, dtc, base::DoNothing());
      DVLOG(1) << "ModelLoadManager: Stopped " << dtc->name();
    }
  }

  desired_types_.Clear();
  loaded_types_.Clear();
}

void ModelLoadManager::ModelLoadCallback(ModelType type,
                                         const SyncError& error) {
  DVLOG(1) << "ModelLoadManager: ModelLoadCallback for "
           << ModelTypeToString(type);

  if (error.IsSet()) {
    DVLOG(1) << "ModelLoadManager: Type encountered an error.";
    desired_types_.Remove(type);
    DataTypeController* dtc = controllers_->find(type)->second.get();
    StopDatatypeImpl(error, ShutdownReason::STOP_SYNC_AND_KEEP_DATA, dtc,
                     base::DoNothing());
    NotifyDelegateIfReadyForConfigure();
    return;
  }

  // This happens when slow loading type is disabled by new configuration or
  // the model came unready during loading.
  if (!desired_types_.Has(type))
    return;

  DCHECK(!loaded_types_.Has(type));
  loaded_types_.Put(type);
  NotifyDelegateIfReadyForConfigure();
}

void ModelLoadManager::NotifyDelegateIfReadyForConfigure() {
  if (notified_about_ready_for_configure_)
    return;

  if (!loaded_types_.HasAll(desired_types_)) {
    // At least one type is not ready.
    return;
  }

  notified_about_ready_for_configure_ = true;
  delegate_->OnAllDataTypesReadyForConfigure();
}

}  // namespace syncer
