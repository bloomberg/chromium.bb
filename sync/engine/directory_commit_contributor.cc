// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/directory_commit_contributor.h"

#include "sync/engine/directory_commit_contribution.h"

namespace syncer {

DirectoryCommitContributor::DirectoryCommitContributor(
    syncable::Directory* dir,
    ModelType type)
    : dir_(dir),
      type_(type) {}

DirectoryCommitContributor::~DirectoryCommitContributor() {}

scoped_ptr<CommitContribution>
DirectoryCommitContributor::GetContribution(size_t max_entries) {
  return DirectoryCommitContribution::Build(dir_, type_, max_entries)
      .PassAs<CommitContribution>();
}

}  // namespace syncer
