// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/commit_processor.h"

#include <map>

#include "sync/engine/commit_contribution.h"
#include "sync/engine/commit_contributor.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

typedef std::map<ModelType, size_t> TypeToIndexMap;

CommitProcessor::CommitProcessor(CommitContributorMap* commit_contributor_map)
    : commit_contributor_map_(commit_contributor_map) {}

CommitProcessor::~CommitProcessor() {}

void CommitProcessor::GatherCommitContributions(
    ModelTypeSet commit_types,
    size_t max_entries,
    ContributionMap* contributions) {
  size_t num_entries = 0;
  for (ModelTypeSet::Iterator it = commit_types.First();
       it.Good(); it.Inc()) {
    CommitContributorMap::iterator cm_it =
        commit_contributor_map_->find(it.Get());
    if (cm_it == commit_contributor_map_->end()) {
      NOTREACHED()
          << "Could not find requested type " << ModelTypeToString(it.Get())
          << " in contributor map.";
      continue;
    }
    size_t spaces_remaining = max_entries - num_entries;
    scoped_ptr<CommitContribution> contribution =
        cm_it->second->GetContribution(spaces_remaining);
    if (contribution) {
      num_entries += contribution->GetNumEntries();
      contributions->insert(std::make_pair(it.Get(), contribution.release()));
    }
    if (num_entries >= max_entries) {
      DCHECK_EQ(num_entries, max_entries)
          << "Number of commit entries exceeeds maximum";
      break;
    }
  }
}

}  // namespace syncer
