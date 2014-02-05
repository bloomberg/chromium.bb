// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_directory_update_handler.h"

#include "sync/engine/conflict_resolver.h"
#include "sync/engine/process_updates_util.h"
#include "sync/engine/update_applicator.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_model_neutral_write_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"

namespace syncer {

using syncable::SYNCER;

SyncDirectoryUpdateHandler::SyncDirectoryUpdateHandler(
    syncable::Directory* dir,
    ModelType type,
    scoped_refptr<ModelSafeWorker> worker)
  : dir_(dir),
    type_(type),
    worker_(worker) {}

SyncDirectoryUpdateHandler::~SyncDirectoryUpdateHandler() {}

void SyncDirectoryUpdateHandler::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  dir_->GetDownloadProgress(type_, progress_marker);
}

void SyncDirectoryUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  UpdateSyncEntities(&trans, applicable_updates, status);
  UpdateProgressMarker(progress_marker);
}

void SyncDirectoryUpdateHandler::ApplyUpdates(
    sessions::StatusController* status) {
  if (!IsApplyUpdatesRequired()) {
    return;
  }

  // This will invoke handlers that belong to the model and its thread, so we
  // switch to the appropriate thread before we start this work.
  WorkCallback c = base::Bind(
      &SyncDirectoryUpdateHandler::ApplyUpdatesImpl,
      // We wait until the callback is executed.  We can safely use Unretained.
      base::Unretained(this),
      base::Unretained(status));
  worker_->DoWorkAndWaitUntilDone(c);
}

void SyncDirectoryUpdateHandler::PassiveApplyUpdates(
    sessions::StatusController* status) {
  if (!IsApplyUpdatesRequired()) {
    return;
  }

  // Just do the work here instead of deferring to another thread.
  ApplyUpdatesImpl(status);
}

SyncerError SyncDirectoryUpdateHandler::ApplyUpdatesImpl(
    sessions::StatusController* status) {
  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir_);

  std::vector<int64> handles;
  dir_->GetUnappliedUpdateMetaHandles(
      &trans,
      FullModelTypeSet(type_),
      &handles);

  // First set of update application passes.
  UpdateApplicator applicator(dir_->GetCryptographer(&trans));
  applicator.AttemptApplications(&trans, handles);
  status->increment_num_updates_applied_by(applicator.updates_applied());
  status->increment_num_hierarchy_conflicts_by(
      applicator.hierarchy_conflicts());
  status->increment_num_encryption_conflicts_by(
      applicator.encryption_conflicts());

  if (applicator.simple_conflict_ids().size() != 0) {
    // Resolve the simple conflicts we just detected.
    ConflictResolver resolver;
    resolver.ResolveConflicts(&trans,
                              dir_->GetCryptographer(&trans),
                              applicator.simple_conflict_ids(),
                              status);

    // Conflict resolution sometimes results in more updates to apply.
    handles.clear();
    dir_->GetUnappliedUpdateMetaHandles(
        &trans,
        FullModelTypeSet(type_),
        &handles);

    UpdateApplicator conflict_applicator(dir_->GetCryptographer(&trans));
    conflict_applicator.AttemptApplications(&trans, handles);

    // We count the number of updates from both applicator passes.
    status->increment_num_updates_applied_by(
        conflict_applicator.updates_applied());

    // Encryption conflicts should remain unchanged by the resolution of simple
    // conflicts.  Those can only be solved by updating our nigori key bag.
    DCHECK_EQ(conflict_applicator.encryption_conflicts(),
              applicator.encryption_conflicts());

    // Hierarchy conflicts should also remain unchanged, for reasons that are
    // more subtle.  Hierarchy conflicts exist when the application of a pending
    // update from the server would make the local folder hierarchy
    // inconsistent.  The resolution of simple conflicts could never affect the
    // hierarchy conflicting item directly, because hierarchy conflicts are not
    // processed by the conflict resolver.  It could, in theory, modify the
    // local hierarchy on which hierarchy conflict detection depends.  However,
    // the conflict resolution algorithm currently in use does not allow this.
    DCHECK_EQ(conflict_applicator.hierarchy_conflicts(),
              applicator.hierarchy_conflicts());

    // There should be no simple conflicts remaining.  We know this because the
    // resolver should have resolved all the conflicts we detected last time
    // and, by the two previous assertions, that no conflicts have been
    // downgraded from encryption or hierarchy down to simple.
    DCHECK(conflict_applicator.simple_conflict_ids().empty());
  }

  return SYNCER_OK;
}

bool SyncDirectoryUpdateHandler::IsApplyUpdatesRequired() {
  if (IsControlType(type_)) {
    return false;  // We don't process control types here.
  }

  return dir_->TypeHasUnappliedUpdates(type_);
}

void SyncDirectoryUpdateHandler::UpdateSyncEntities(
    syncable::ModelNeutralWriteTransaction* trans,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  ProcessDownloadedUpdates(dir_, trans, type_, applicable_updates, status);
}

void SyncDirectoryUpdateHandler::UpdateProgressMarker(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  int field_number = progress_marker.data_type_id();
  ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
  if (!IsRealDataType(model_type) || type_ != model_type) {
    NOTREACHED()
        << "Update handler of type " << ModelTypeToString(type_)
        << " asked to process progress marker with invalid type "
        << field_number;
  }
  dir_->SetDownloadProgress(type_, progress_marker);
}

}  // namespace syncer
