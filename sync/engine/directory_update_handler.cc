// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/directory_update_handler.h"

#include "sync/engine/conflict_resolver.h"
#include "sync/engine/process_updates_util.h"
#include "sync/engine/update_applicator.h"
#include "sync/sessions/directory_type_debug_info_emitter.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_model_neutral_write_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"

namespace syncer {

using syncable::SYNCER;

DirectoryUpdateHandler::DirectoryUpdateHandler(
    syncable::Directory* dir,
    ModelType type,
    scoped_refptr<ModelSafeWorker> worker,
    DirectoryTypeDebugInfoEmitter* debug_info_emitter)
  : dir_(dir),
    type_(type),
    worker_(worker),
    debug_info_emitter_(debug_info_emitter) {}

DirectoryUpdateHandler::~DirectoryUpdateHandler() {}

void DirectoryUpdateHandler::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  dir_->GetDownloadProgress(type_, progress_marker);
}

void DirectoryUpdateHandler::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  dir_->GetDataTypeContext(&trans, type_, context);
}

SyncerError DirectoryUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  if (mutated_context.has_context()) {
    sync_pb::DataTypeContext local_context;
    dir_->GetDataTypeContext(&trans, type_, &local_context);

    // Only update the local context if it is still relevant. If the local
    // version is higher, it means a local change happened while the mutation
    // was in flight, and the local context takes priority.
    if (mutated_context.version() >= local_context.version() &&
        local_context.context() != mutated_context.context()) {
      dir_->SetDataTypeContext(&trans, type_, mutated_context);
      // TODO(zea): trigger the datatype's UpdateDataTypeContext method.
    } else if (mutated_context.version() < local_context.version()) {
      // A GetUpdates using the old context was in progress when the context was
      // set. Fail this get updates cycle, to force a retry.
      DVLOG(1) << "GU Context conflict detected, forcing GU retry.";
      debug_info_emitter_->EmitUpdateCountersUpdate();
      return DATATYPE_TRIGGERED_RETRY;
    }
  }

  UpdateSyncEntities(&trans, applicable_updates, status);

  if (IsValidProgressMarker(progress_marker)) {
    ExpireEntriesIfNeeded(&trans, progress_marker);
    UpdateProgressMarker(progress_marker);
  }
  debug_info_emitter_->EmitUpdateCountersUpdate();
  return SYNCER_OK;
}

void DirectoryUpdateHandler::ApplyUpdates(sessions::StatusController* status) {
  if (!IsApplyUpdatesRequired()) {
    return;
  }

  // This will invoke handlers that belong to the model and its thread, so we
  // switch to the appropriate thread before we start this work.
  WorkCallback c = base::Bind(
      &DirectoryUpdateHandler::ApplyUpdatesImpl,
      // We wait until the callback is executed.  We can safely use Unretained.
      base::Unretained(this),
      base::Unretained(status));
  worker_->DoWorkAndWaitUntilDone(c);

  debug_info_emitter_->EmitUpdateCountersUpdate();
  debug_info_emitter_->EmitStatusCountersUpdate();
}

void DirectoryUpdateHandler::PassiveApplyUpdates(
    sessions::StatusController* status) {
  if (!IsApplyUpdatesRequired()) {
    return;
  }

  // Just do the work here instead of deferring to another thread.
  ApplyUpdatesImpl(status);

  debug_info_emitter_->EmitUpdateCountersUpdate();
  debug_info_emitter_->EmitStatusCountersUpdate();
}

SyncerError DirectoryUpdateHandler::ApplyUpdatesImpl(
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

  // The old StatusController counters.
  status->increment_num_updates_applied_by(applicator.updates_applied());
  status->increment_num_hierarchy_conflicts_by(
      applicator.hierarchy_conflicts());
  status->increment_num_encryption_conflicts_by(
      applicator.encryption_conflicts());

  // The new UpdateCounter counters.
  UpdateCounters* counters = debug_info_emitter_->GetMutableUpdateCounters();
  counters->num_updates_applied += applicator.updates_applied();
  counters->num_hierarchy_conflict_application_failures =
      applicator.hierarchy_conflicts();
  counters->num_encryption_conflict_application_failures +=
      applicator.encryption_conflicts();

  if (applicator.simple_conflict_ids().size() != 0) {
    // Resolve the simple conflicts we just detected.
    ConflictResolver resolver;
    resolver.ResolveConflicts(&trans,
                              dir_->GetCryptographer(&trans),
                              applicator.simple_conflict_ids(),
                              status,
                              counters);

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
    counters->num_updates_applied += conflict_applicator.updates_applied();

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

bool DirectoryUpdateHandler::IsApplyUpdatesRequired() {
  if (IsControlType(type_)) {
    return false;  // We don't process control types here.
  }

  return dir_->TypeHasUnappliedUpdates(type_);
}

void DirectoryUpdateHandler::UpdateSyncEntities(
    syncable::ModelNeutralWriteTransaction* trans,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  UpdateCounters* counters = debug_info_emitter_->GetMutableUpdateCounters();
  counters->num_updates_received += applicable_updates.size();
  ProcessDownloadedUpdates(dir_, trans, type_,
                           applicable_updates, status, counters);
}

bool DirectoryUpdateHandler::IsValidProgressMarker(
    const sync_pb::DataTypeProgressMarker& progress_marker) const {
  int field_number = progress_marker.data_type_id();
  ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
  if (!IsRealDataType(model_type) || type_ != model_type) {
    NOTREACHED()
        << "Update handler of type " << ModelTypeToString(type_)
        << " asked to process progress marker with invalid type "
        << field_number;
    return false;
  }
  return true;
}

void DirectoryUpdateHandler::UpdateProgressMarker(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  if (progress_marker.has_gc_directive() || !cached_gc_directive_) {
    dir_->SetDownloadProgress(type_, progress_marker);
  } else {
    sync_pb::DataTypeProgressMarker merged_marker = progress_marker;
    merged_marker.mutable_gc_directive()->CopyFrom(*cached_gc_directive_);
    dir_->SetDownloadProgress(type_, merged_marker);
  }
}

void DirectoryUpdateHandler::ExpireEntriesIfNeeded(
    syncable::ModelNeutralWriteTransaction* trans,
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  if (!cached_gc_directive_) {
    sync_pb::DataTypeProgressMarker current_marker;
    GetDownloadProgress(&current_marker);
    if (current_marker.has_gc_directive()) {
      cached_gc_directive_.reset(new sync_pb::GarbageCollectionDirective(
          current_marker.gc_directive()));
    }
  }

  if (!progress_marker.has_gc_directive())
    return;

  const sync_pb::GarbageCollectionDirective& new_gc_directive =
      progress_marker.gc_directive();

  if (new_gc_directive.has_version_watermark() &&
      (!cached_gc_directive_ ||
          cached_gc_directive_->version_watermark() <
              new_gc_directive.version_watermark())) {
    ExpireEntriesByVersion(dir_, trans, type_,
                           new_gc_directive.version_watermark());
  }

  cached_gc_directive_.reset(
      new sync_pb::GarbageCollectionDirective(new_gc_directive));
}

}  // namespace syncer
