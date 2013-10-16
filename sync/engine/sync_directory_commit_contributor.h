// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNC_DIRECTORY_COMMIT_CONTRIBUTOR_H_
#define SYNC_ENGINE_SYNC_DIRECTORY_COMMIT_CONTRIBUTOR_H_

#include <map>

#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class SyncDirectoryCommitContribution;

namespace syncable {
class Directory;
}

// This class represents the syncable::Directory as a source of items to commit
// to the sync server.
//
// Each instance of this class represents a particular type within the
// syncable::Directory.  When asked, it will iterate through the directory, grab
// any items of its type that are ready for commit, and return them in the form
// of a SyncDirectoryCommitContribution.
class SyncDirectoryCommitContributor {
 public:
  SyncDirectoryCommitContributor(syncable::Directory* dir, ModelType type);
  ~SyncDirectoryCommitContributor();

  SyncDirectoryCommitContribution* GetContribution(size_t max_entries);

 private:
  syncable::Directory* dir_;
  ModelType type_;
};

// TODO(rlarocque): Find a better place for this definition.
typedef std::map<ModelType, SyncDirectoryCommitContributor*>
    CommitContributorMap;

}  // namespace

#endif  // SYNC_ENGINE_SYNC_DIRECTORY_COMMIT_CONTRIBUTOR_H_
