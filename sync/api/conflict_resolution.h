// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_CONFLICT_RESOLUTION_H_
#define SYNC_API_CONFLICT_RESOLUTION_H_

#include "base/memory/scoped_ptr.h"
#include "sync/api/entity_data.h"

namespace syncer_v2 {

// A simple class to represent the resolution of a data conflict. We either:
// 1) Use the local client data and update the server.
// 2) Use the remote server data and update the client.
// 3) Use newly created data and update both.
class SYNC_EXPORT ConflictResolution {
 public:
  // This enum is used in histograms.xml and entries shouldn't be renumbered or
  // removed. New entries must be added at the end, before TYPE_SIZE.
  enum Type {
    CHANGES_MATCH,  // Exists for logging purposes.
    USE_LOCAL,
    USE_REMOTE,
    USE_NEW,
    TYPE_SIZE,
  };

  // Convenience functions for brevity.
  static ConflictResolution UseLocal();
  static ConflictResolution UseRemote();
  static ConflictResolution UseNew(scoped_ptr<EntityData> data);

  // Move constructor since we can't copy a scoped_ptr.
  ConflictResolution(ConflictResolution&& other);
  ~ConflictResolution();

  Type type() const { return type_; }

  // Get the data for USE_NEW, or nullptr. Can only be called once.
  scoped_ptr<EntityData> ExtractData();

 private:
  ConflictResolution(Type type, scoped_ptr<EntityData> data);

  const Type type_;
  scoped_ptr<EntityData> data_;

  DISALLOW_COPY_AND_ASSIGN(ConflictResolution);
};

}  // namespace syncer_v2

#endif  // SYNC_API_CONFLICT_RESOLUTION_H_
