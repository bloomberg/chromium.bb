// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/directory_type_debug_info_emitter.h"

#include "sync/internal_api/public/sessions/status_counters.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/syncable/syncable_read_transaction.h"

namespace syncer {

DirectoryTypeDebugInfoEmitter::DirectoryTypeDebugInfoEmitter(
    syncable::Directory* directory,
    syncer::ModelType type,
    ObserverList<TypeDebugInfoObserver>* observers)
    : directory_(directory),
      type_(type),
      type_debug_info_observers_(observers) {}

DirectoryTypeDebugInfoEmitter::DirectoryTypeDebugInfoEmitter(
    ModelType type,
    ObserverList<TypeDebugInfoObserver>* observers)
    : directory_(NULL),
      type_(type),
      type_debug_info_observers_(observers) {}

DirectoryTypeDebugInfoEmitter::~DirectoryTypeDebugInfoEmitter() {}

scoped_ptr<base::ListValue> DirectoryTypeDebugInfoEmitter::GetAllNodes() {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  scoped_ptr<base::ListValue> nodes(
      directory_->GetNodeDetailsForType(&trans, type_));
  return nodes.Pass();
}

const CommitCounters& DirectoryTypeDebugInfoEmitter::GetCommitCounters() const {
  return commit_counters_;
}

CommitCounters* DirectoryTypeDebugInfoEmitter::GetMutableCommitCounters() {
  return &commit_counters_;
}

void DirectoryTypeDebugInfoEmitter::EmitCommitCountersUpdate() {
  FOR_EACH_OBSERVER(TypeDebugInfoObserver, (*type_debug_info_observers_),
                    OnCommitCountersUpdated(type_, commit_counters_));
}

const UpdateCounters& DirectoryTypeDebugInfoEmitter::GetUpdateCounters() const {
  return update_counters_;
}

UpdateCounters* DirectoryTypeDebugInfoEmitter::GetMutableUpdateCounters() {
  return &update_counters_;
}

void DirectoryTypeDebugInfoEmitter::EmitUpdateCountersUpdate() {
  FOR_EACH_OBSERVER(TypeDebugInfoObserver, (*type_debug_info_observers_),
                    OnUpdateCountersUpdated(type_, update_counters_));
}

void DirectoryTypeDebugInfoEmitter::EmitStatusCountersUpdate() {
  // TODO(rlarocque): Implement this.  Part of crbug.com/328606.
}

}  // namespace syncer
