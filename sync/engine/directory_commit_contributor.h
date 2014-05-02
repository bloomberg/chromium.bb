// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DIRECTORY_COMMIT_CONTRIBUTOR_H_
#define SYNC_ENGINE_DIRECTORY_COMMIT_CONTRIBUTOR_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "sync/engine/commit_contributor.h"
#include "sync/engine/directory_commit_contribution.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

namespace syncable {
class Directory;
}

class DirectoryTypeDebugInfoEmitter;

// This class represents the syncable::Directory as a source of items to commit
// to the sync server.
//
// Each instance of this class represents a particular type within the
// syncable::Directory.  When asked, it will iterate through the directory, grab
// any items of its type that are ready for commit, and return them in the form
// of a DirectoryCommitContribution.
class DirectoryCommitContributor : public CommitContributor {
 public:
  DirectoryCommitContributor(syncable::Directory* dir,
                             ModelType type,
                             DirectoryTypeDebugInfoEmitter* debug_info_emitter);
  virtual ~DirectoryCommitContributor();

  virtual scoped_ptr<CommitContribution> GetContribution(
      size_t max_entries) OVERRIDE;

 private:
  syncable::Directory* dir_;
  ModelType type_;

  DirectoryTypeDebugInfoEmitter* debug_info_emitter_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryCommitContributor);
};

}  // namespace

#endif  // SYNC_ENGINE_DIRECTORY_COMMIT_CONTRIBUTOR_H_
