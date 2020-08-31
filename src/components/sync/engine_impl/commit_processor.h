// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_PROCESSOR_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine_impl/commit.h"
#include "components/sync/engine_impl/model_type_registry.h"

namespace syncer {

class CommitContributor;

// This class manages the set of per-type committer objects.
//
// It owns these types and hides the details of iterating over all of them.
// It is a logic error if the supplied set of types contains a type which was
// not previously registered.
class CommitProcessor {
 public:
  // |commit_types| must contain NIGORI. |commit_contributor_map| must be not
  // null and must outlive this object.
  CommitProcessor(ModelTypeSet commit_types,
                  CommitContributorMap* commit_contributor_map);
  ~CommitProcessor();

  // Gathers a set of contributions to be used to populate a commit message.
  //
  // For each of the |commit_types| in this CommitProcessor's CommitContributor
  // map, gather any entries queued for commit into CommitContributions.  The
  // total number of entries in all the returned CommitContributions shall not
  // exceed |max_entries|.
  // Returns no contribution if previous call collected them from all datatypes
  // and total number of collected entries was less than |max_entries|.
  // Note: |cookie_jar_mismatch| and |cookie_jar_empty| are used only for
  // metrics recording purposes specific to the SESSIONS type.
  Commit::ContributionMap GatherCommitContributions(size_t max_entries,
                                                    bool cookie_jar_mismatch,
                                                    bool cookie_jar_empty);

 private:
  // Gathers commit contributions for an individual datatype and populates
  // |*contributions|. Returns the number of entries added.
  size_t GatherCommitContributionsForType(
      ModelType type,
      size_t max_entries,
      bool cookie_jar_mismatch,
      bool cookie_jar_empty,
      Commit::ContributionMap* contributions);

  // Gathers commit contributions for |types| and populates |*contributions|.
  // Returns the number of entries added.
  size_t GatherCommitContributionsForTypes(
      ModelTypeSet types,
      size_t max_entries,
      bool cookie_jar_mismatch,
      bool cookie_jar_empty,
      Commit::ContributionMap* contributions);

  const ModelTypeSet commit_types_;

  // A map of 'commit contributors', one for each enabled type.
  CommitContributorMap* commit_contributor_map_;
  bool gathered_all_contributions_;

  DISALLOW_COPY_AND_ASSIGN(CommitProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_PROCESSOR_H_
