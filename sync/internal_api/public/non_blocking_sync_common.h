// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_
#define SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "sync/api/entity_data.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace syncer_v2 {

static const int64_t kUncommittedVersion = -1;

struct SYNC_EXPORT CommitRequestData {
  CommitRequestData();
  ~CommitRequestData();

  EntityDataPtr entity;

  // Strictly incrementing number for in-progress commits.  More information
  // about its meaning can be found in comments in the files that make use of
  // this struct.
  int64_t sequence_number = 0;
  int64_t base_version = 0;
};

struct SYNC_EXPORT CommitResponseData {
  CommitResponseData();
  ~CommitResponseData();

  std::string id;
  std::string client_tag_hash;
  int64_t sequence_number = 0;
  int64_t response_version = 0;
};

struct SYNC_EXPORT UpdateResponseData {
  UpdateResponseData();
  ~UpdateResponseData();

  EntityDataPtr entity;

  int64_t response_version = 0;
  std::string encryption_key_name;
};

typedef std::vector<CommitRequestData> CommitRequestDataList;
typedef std::vector<CommitResponseData> CommitResponseDataList;
typedef std::vector<UpdateResponseData> UpdateResponseDataList;

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_
