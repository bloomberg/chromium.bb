// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DIRECTORY_TYPE_DEBUG_INFO_EMITTER_H_
#define SYNC_ENGINE_DIRECTORY_TYPE_DEBUG_INFO_EMITTER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/syncable/directory.h"

namespace syncer {

// Supports debugging requests for a certain directory type.
class DirectoryTypeDebugInfoEmitter {
 public:
  DirectoryTypeDebugInfoEmitter(syncable::Directory* directory,
                                syncer::ModelType type);
  virtual ~DirectoryTypeDebugInfoEmitter();

  // Returns a ListValue representation of all known nodes of this type.
  scoped_ptr<base::ListValue> GetAllNodes();

 private:
  syncable::Directory* directory_;
  ModelType type_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryTypeDebugInfoEmitter);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_DIRECTORY_TYPE_DEBUG_INFO_EMITTER_H_
