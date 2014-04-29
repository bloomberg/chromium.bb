// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_COMMIT_COUNTERS_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_COMMIT_COUNTERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/base/sync_export.h"

namespace syncer {

// A class to maintain counts related to sync commit requests and responses.
struct SYNC_EXPORT_PRIVATE CommitCounters {
  CommitCounters();
  ~CommitCounters();

  scoped_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

  int num_commits_attempted;
  int num_commits_success;
  int num_commits_conflict;
  int num_commits_error;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_COMMIT_COUNTERS_H_
