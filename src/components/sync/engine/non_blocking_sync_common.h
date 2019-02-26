// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NON_BLOCKING_SYNC_COMMON_H_
#define COMPONENTS_SYNC_ENGINE_NON_BLOCKING_SYNC_COMMON_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

static const int64_t kUncommittedVersion = -1;

struct CommitRequestData {
  CommitRequestData();
  CommitRequestData(const CommitRequestData& other);
  ~CommitRequestData();

  // Fields sent to the sync server.
  EntityDataPtr entity;
  int64_t base_version = 0;

  // Fields not sent to the sync server. However, they are kept to be sent back
  // to the processor in the response.

  // Strictly incrementing number for in-progress commits.
  // More information about its meaning can be found in comments in the files
  // that make use of this struct.
  int64_t sequence_number = 0;
  std::string specifics_hash;
  base::Time unsynced_time;
};

struct CommitResponseData {
  CommitResponseData();
  CommitResponseData(const CommitResponseData& other);
  ~CommitResponseData();

  std::string id;
  // The sync id that was sent in the request. Non-empty only if different from
  // |id|. It could be different because the server can change the sync id
  // (e.g. for newly created bookmarks),
  std::string id_in_request;
  std::string client_tag_hash;
  int64_t sequence_number = 0;
  int64_t response_version = 0;
  std::string specifics_hash;
  base::Time unsynced_time;
};

struct UpdateResponseData {
  UpdateResponseData();
  UpdateResponseData(const UpdateResponseData& other);
  ~UpdateResponseData();

  EntityDataPtr entity;

  int64_t response_version = 0;
  std::string encryption_key_name;
};

using CommitRequestDataList = std::vector<CommitRequestData>;
using CommitResponseDataList = std::vector<CommitResponseData>;
using UpdateResponseDataList = std::vector<UpdateResponseData>;

// Returns the estimate of dynamically allocated memory in bytes.
size_t EstimateMemoryUsage(const CommitRequestData& value);
size_t EstimateMemoryUsage(const UpdateResponseData& value);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NON_BLOCKING_SYNC_COMMON_H_
