// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_directory_commit_contributor.h"

#include "sync/engine/sync_directory_commit_contribution.h"

namespace syncer {

SyncDirectoryCommitContributor::SyncDirectoryCommitContributor(
    syncable::Directory* dir,
    ModelType type)
    : dir_(dir),
      type_(type) {}

SyncDirectoryCommitContributor::~SyncDirectoryCommitContributor() {}

SyncDirectoryCommitContribution*
SyncDirectoryCommitContributor::GetContribution(size_t max_entries) {
  return SyncDirectoryCommitContribution::Build(dir_, type_, max_entries);
}

}  // namespace syncer
