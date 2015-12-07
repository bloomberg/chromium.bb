// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_STATUS_COUNTERS_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_STATUS_COUNTERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/base/sync_export.h"

namespace syncer {

// A class to maintain counts related to the current status of a sync type.
struct SYNC_EXPORT_PRIVATE StatusCounters {
  StatusCounters();
  ~StatusCounters();

  scoped_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

  size_t num_entries;
  size_t num_entries_and_tombstones;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_STATUS_COUNTERS_H_
