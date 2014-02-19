// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_COMMIT_PROCESSOR_H_
#define SYNC_ENGINE_COMMIT_PROCESSOR_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/sessions/model_type_registry.h"

namespace syncer {

namespace syncable {
class Directory;
}  // namespace syncable

class CommitContributor;
class CommitContribution;

// This class manages the set of per-type committer objects.
//
// It owns these types and hides the details of iterating over all of them.
// Many methods allow the caller to specify a subset of types on which the
// operation is to be applied.  It is a logic error if the supplied set of types
// contains a type which was not previously registered.
class SYNC_EXPORT_PRIVATE CommitProcessor {
 public:
  typedef std::map<ModelType, CommitContribution*> ContributionMap;

  // Contructs a CommitProcessor from a map of CommitContributors.
  // The CommitProcessor does not own this map.
  explicit CommitProcessor(CommitContributorMap* commit_contributor_map);
  ~CommitProcessor();

  // Gathers a set of contributions to be used to populate a commit message.
  //
  // For each of the |commit_types| in this CommitProcessor's CommitContributor
  // map, gather any entries queued for commit into CommitContributions.  The
  // total number of entries in all the returned CommitContributions shall not
  // exceed |max_entries|.
  void GatherCommitContributions(
      ModelTypeSet commit_types,
      size_t max_entries,
      ContributionMap* contributions);

 private:
  // A map of 'commit contributors', one for each enabled type.
  CommitContributorMap* commit_contributor_map_;

  DISALLOW_COPY_AND_ASSIGN(CommitProcessor);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_COMMIT_PROCESSOR_H_
