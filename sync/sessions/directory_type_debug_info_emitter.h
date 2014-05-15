// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SESSIONS_DIRECTORY_TYPE_DEBUG_INFO_EMITTER_H_
#define SYNC_SESSIONS_DIRECTORY_TYPE_DEBUG_INFO_EMITTER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/sessions/commit_counters.h"
#include "sync/internal_api/public/sessions/update_counters.h"
#include "sync/syncable/directory.h"

namespace syncer {

class DirectoryCommitContributor;
class TypeDebugInfoObserver;

// Supports various kinds of debugging requests for a certain directory type.
//
// The GetAllNodes() function is used to help export a snapshot of all current
// nodes to its caller.  The complexity required to manage the request mostly
// lives elsewhere.
//
// The Emit*() functions send updates to registered TypeDebugInfoObservers.
// The DirectoryTypeDebugInfoEmitter does not directly own that list; it is
// managed by the ModelTypeRegistry.
//
// For Update and Commit counters, the job of keeping the counters up to date
// is delegated to the UpdateHandler and CommitContributors.  For the Stats
// counters, the emitter will use its type_ and directory_ members to fetch all
// the required information on demand.
class SYNC_EXPORT_PRIVATE DirectoryTypeDebugInfoEmitter {
 public:
  // Standard constructor for non-tests.
  //
  // The |directory| and |observers| arguments are not owned.  Both may be
  // modified outside of this object and both are expected to outlive this
  // object.
  DirectoryTypeDebugInfoEmitter(
      syncable::Directory* directory,
      syncer::ModelType type,
      ObserverList<TypeDebugInfoObserver>* observers);

  // A simple constructor for tests.  Should not be used in real code.
  DirectoryTypeDebugInfoEmitter(
      ModelType type,
      ObserverList<TypeDebugInfoObserver>* observers);

  virtual ~DirectoryTypeDebugInfoEmitter();

  // Returns a ListValue representation of all known nodes of this type.
  scoped_ptr<base::ListValue> GetAllNodes();

  // Returns a reference to the current commit counters.
  const CommitCounters& GetCommitCounters() const;

  // Allows others to mutate the commit counters.
  CommitCounters* GetMutableCommitCounters();

  // Triggerss a commit counters update to registered observers.
  void EmitCommitCountersUpdate();

  // Returns a reference to the current update counters.
  const UpdateCounters& GetUpdateCounters() const;

  // Allows others to mutate the update counters.
  UpdateCounters* GetMutableUpdateCounters();

  // Triggers an update counters update to registered observers.
  void EmitUpdateCountersUpdate();

  // Triggers a status counters update to registered observers.
  void EmitStatusCountersUpdate();

 private:
  syncable::Directory* directory_;

  const ModelType type_;

  CommitCounters commit_counters_;
  UpdateCounters update_counters_;

  // Because there are so many emitters that come into and out of existence, it
  // doesn't make sense to have them manage their own observer list.  They all
  // share one observer list that is provided by their owner and which is
  // guaranteed to outlive them.
  ObserverList<TypeDebugInfoObserver>* type_debug_info_observers_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryTypeDebugInfoEmitter);
};

}  // namespace syncer

#endif  // SYNC_SESSIONS_DIRECTORY_TYPE_DEBUG_INFO_EMITTER_H_
