// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/directory_type_debug_info_emitter.h"

#include "sync/syncable/syncable_read_transaction.h"

namespace syncer {

DirectoryTypeDebugInfoEmitter::DirectoryTypeDebugInfoEmitter(
    syncable::Directory* directory,
    syncer::ModelType type)
    : directory_(directory),
      type_(type) {}

DirectoryTypeDebugInfoEmitter::~DirectoryTypeDebugInfoEmitter() {}

scoped_ptr<base::ListValue> DirectoryTypeDebugInfoEmitter::GetAllNodes() {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  scoped_ptr<base::ListValue> nodes(
      directory_->GetNodeDetailsForType(&trans, type_));
  return nodes.Pass();
}

}  // namespace syncer
