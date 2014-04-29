// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_UPDATE_COUNTERS_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_UPDATE_COUNTERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/base/sync_export.h"

namespace syncer {

// A class to maintain counts related to the update requests and responses for
// a particular sync type.
struct SYNC_EXPORT_PRIVATE UpdateCounters {
  UpdateCounters();
  ~UpdateCounters();

  scoped_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

  int num_updates_received;
  int num_reflected_updates_received;
  int num_tombstone_updates_received;

  int num_updates_applied;
  int num_hierarchy_conflict_application_failures;
  int num_encryption_conflict_application_failures;

  int num_server_overwrites;
  int num_local_overwrites;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_UPDATE_COUNTERS_H_
