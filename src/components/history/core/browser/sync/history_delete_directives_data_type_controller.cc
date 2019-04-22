// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_delete_directives_data_type_controller.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace browser_sync {

HistoryDeleteDirectivesDataTypeController::
    HistoryDeleteDirectivesDataTypeController(const base::Closure& dump_stack,
                                              syncer::SyncService* sync_service,
                                              syncer::SyncClient* sync_client)
    : syncer::AsyncDirectoryTypeController(
          syncer::HISTORY_DELETE_DIRECTIVES,
          dump_stack,
          sync_service,
          sync_client,
          syncer::GROUP_UI,
          base::ThreadTaskRunnerHandle::Get()) {}

HistoryDeleteDirectivesDataTypeController::
    ~HistoryDeleteDirectivesDataTypeController() {}

bool HistoryDeleteDirectivesDataTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return !sync_service()->GetUserSettings()->IsEncryptEverythingEnabled();
}

bool HistoryDeleteDirectivesDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  if (DisableTypeIfNecessary())
    return false;
  sync_service()->AddObserver(this);
  return true;
}

void HistoryDeleteDirectivesDataTypeController::StopModels() {
  DCHECK(CalledOnValidThread());
  sync_service()->RemoveObserver(this);
}

void HistoryDeleteDirectivesDataTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DisableTypeIfNecessary();
}

bool HistoryDeleteDirectivesDataTypeController::DisableTypeIfNecessary() {
  DCHECK(CalledOnValidThread());
  if (!sync_service()->IsSyncFeatureActive())
    return false;

  if (ReadyForStart())
    return false;

  sync_service()->RemoveObserver(this);
  syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                          "Delete directives not supported with encryption.",
                          type());
  CreateErrorHandler()->OnUnrecoverableError(error);
  return true;
}

}  // namespace browser_sync
