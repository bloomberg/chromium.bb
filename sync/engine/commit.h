// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_COMMIT_H_
#define SYNC_ENGINE_COMMIT_H_

#include <map>

#include "base/stl_util.h"
#include "sync/base/sync_export.h"
#include "sync/engine/commit_contribution.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/extensions_activity.h"

namespace syncer {

namespace sessions {
class StatusController;
class SyncSession;
}

class CommitProcessor;
class Syncer;

// This class wraps the actions related to building and executing a single
// commit operation.
//
// This class' most important responsibility is to manage the ContributionsMap.
// This class serves as a container for those objects.  Although it would have
// been acceptable to let this class be a dumb container object, it turns out
// that there was no other convenient place to put the Init() and
// PostAndProcessCommitResponse() functions.  So they ended up here.
class SYNC_EXPORT_PRIVATE Commit {
 public:
  Commit(
      const std::map<ModelType, CommitContribution*>&
          contributions,
      const sync_pb::ClientToServerMessage& message,
      ExtensionsActivity::Records extensions_activity_buffer);

  // This destructor will DCHECK if CleanUp() has not been called.
  ~Commit();

  static Commit* Init(
      ModelTypeSet requested_types,
      ModelTypeSet enabled_types,
      size_t max_entries,
      const std::string& account_name,
      const std::string& cache_guid,
      CommitProcessor* commit_processor,
      ExtensionsActivity* extensions_activity);

  SyncerError PostAndProcessResponse(
      sessions::SyncSession* session,
      sessions::StatusController* status,
      ExtensionsActivity* extensions_activity);

  // Cleans up state associated with this commit.  Must be called before the
  // destructor.
  void CleanUp();

 private:
  typedef std::map<ModelType, CommitContribution*> ContributionMap;

  ContributionMap contributions_;
  STLValueDeleter<ContributionMap> deleter_;

  sync_pb::ClientToServerMessage message_;
  sync_pb::ClientToServerResponse response_;
  ExtensionsActivity::Records extensions_activity_buffer_;

  // Debug only flag used to indicate if it's safe to destruct the object.
  bool cleaned_up_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_COMMIT_H_
