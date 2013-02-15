// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/apply_updates_and_resolve_conflicts_command.h"

#include "base/location.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/update_applicator.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"

namespace syncer {

using sessions::SyncSession;

ApplyUpdatesAndResolveConflictsCommand::
    ApplyUpdatesAndResolveConflictsCommand() {}
ApplyUpdatesAndResolveConflictsCommand::
    ~ApplyUpdatesAndResolveConflictsCommand() {}

std::set<ModelSafeGroup>
ApplyUpdatesAndResolveConflictsCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  std::set<ModelSafeGroup> groups_with_unapplied_updates;

  FullModelTypeSet server_types_with_unapplied_updates;
  {
    syncable::Directory* dir = session.context()->directory();
    syncable::ReadTransaction trans(FROM_HERE, dir);
    server_types_with_unapplied_updates =
        dir->GetServerTypesWithUnappliedUpdates(&trans);
  }

  for (FullModelTypeSet::Iterator it =
           server_types_with_unapplied_updates.First(); it.Good(); it.Inc()) {
    groups_with_unapplied_updates.insert(
        GetGroupForModelType(it.Get(), session.context()->routing_info()));
  }

  return groups_with_unapplied_updates;
}

SyncerError ApplyUpdatesAndResolveConflictsCommand::ModelChangingExecuteImpl(
    SyncSession* session) {
  sessions::StatusController* status = session->mutable_status_controller();
  syncable::Directory* dir = session->context()->directory();
  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);

  // Compute server types with unapplied updates that fall under our
  // group restriction.
  const FullModelTypeSet server_types_with_unapplied_updates =
      dir->GetServerTypesWithUnappliedUpdates(&trans);
  FullModelTypeSet server_type_restriction;
  for (FullModelTypeSet::Iterator it =
           server_types_with_unapplied_updates.First(); it.Good(); it.Inc()) {
    if (GetGroupForModelType(it.Get(), session->context()->routing_info()) ==
        status->group_restriction()) {
      server_type_restriction.Put(it.Get());
    }
  }

  // Don't process control type updates here.  They will be handled elsewhere.
  FullModelTypeSet control_types = ToFullModelTypeSet(ControlTypes());
  server_type_restriction.RemoveAll(control_types);

  std::vector<int64> handles;
  dir->GetUnappliedUpdateMetaHandles(
      &trans, server_type_restriction, &handles);

  // First set of update application passes.
  UpdateApplicator applicator(
      dir->GetCryptographer(&trans),
      session->context()->routing_info(),
      status->group_restriction());
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
                              dir->GetCryptographer(&trans),
                              applicator.simple_conflict_ids(),
                              status);

    // Conflict resolution sometimes results in more updates to apply.
    handles.clear();
    dir->GetUnappliedUpdateMetaHandles(
        &trans, server_type_restriction, &handles);

    UpdateApplicator conflict_applicator(
        dir->GetCryptographer(&trans),
        session->context()->routing_info(),
        status->group_restriction());
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

}  // namespace syncer
