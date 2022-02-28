// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

UserEventModelTypeController::UserEventModelTypeController(
    SyncService* sync_service,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode)
    : ModelTypeController(syncer::USER_EVENTS,
                          std::move(delegate_for_full_sync_mode)),
      sync_service_(sync_service) {
  DCHECK(sync_service_);
  sync_service_->AddObserver(this);
}

UserEventModelTypeController::~UserEventModelTypeController() {
  sync_service_->RemoveObserver(this);
}

void UserEventModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                        StopCallback callback) {
  DCHECK(CalledOnValidThread());
  switch (shutdown_reason) {
    case syncer::ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
      // Special case: For USER_EVENT, we want to clear all data even when Sync
      // is stopped temporarily.
      shutdown_reason = syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA;
      break;
    case syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
    case syncer::ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      break;
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
}

DataTypeController::PreconditionState
UserEventModelTypeController::GetPreconditionState() const {
  return sync_service_->GetUserSettings()->IsUsingExplicitPassphrase()
             ? PreconditionState::kMustStopAndClearData
             : PreconditionState::kPreconditionsMet;
}

void UserEventModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  sync->DataTypePreconditionChanged(type());
}

}  // namespace syncer
