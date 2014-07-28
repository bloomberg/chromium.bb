// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_
#define SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

static const int64 kUncommittedVersion = -1;

// Data-type global state that must be accessed and updated on the sync thread,
// but persisted on or through the model thread.
struct SYNC_EXPORT_PRIVATE DataTypeState {
  DataTypeState();
  ~DataTypeState();

  // The latest progress markers received from the server.
  sync_pb::DataTypeProgressMarker progress_marker;

  // A data type context.  Sent to the server in every commit or update
  // request.  May be updated by either by responses from the server or
  // requests made on the model thread.  The interpretation of this value may
  // be data-type specific.  Many data types ignore it.
  sync_pb::DataTypeContext type_context;

  // The ID of the folder node that sits at the top of this type's folder
  // hierarchy.  We keep this around for legacy reasons.  The protocol expects
  // that all nodes of a certain type are children of the same type root
  // entity.  This entity is delivered by the server, and may not be available
  // until the first download cycle has completed.
  std::string type_root_id;

  // A strictly increasing counter used to generate unique values for the
  // client-assigned IDs.  The incrementing and ID assignment happens on the
  // sync thread, but we store the value here so we can pass it back to the
  // model thread for persistence.  This is probably unnecessary for the
  // client-tagged data types supported by non-blocking sync, but we will
  // continue to emulate the directory sync's behavior for now.
  int64 next_client_id;

  // This flag is set to true when the first download cycle is complete.  The
  // ModelTypeSyncProxy should not attempt to commit any items until this
  // flag is set.
  bool initial_sync_done;
};

struct SYNC_EXPORT_PRIVATE CommitRequestData {
  CommitRequestData();
  ~CommitRequestData();

  std::string id;
  std::string client_tag_hash;

  // Strictly incrementing number for in-progress commits.  More information
  // about its meaning can be found in comments in the files that make use of
  // this struct.
  int64 sequence_number;

  int64 base_version;
  base::Time ctime;
  base::Time mtime;
  std::string non_unique_name;
  bool deleted;
  sync_pb::EntitySpecifics specifics;
};

struct SYNC_EXPORT_PRIVATE CommitResponseData {
  CommitResponseData();
  ~CommitResponseData();

  std::string id;
  std::string client_tag_hash;
  int64 sequence_number;
  int64 response_version;
};

struct SYNC_EXPORT_PRIVATE UpdateResponseData {
  UpdateResponseData();
  ~UpdateResponseData();

  std::string id;
  std::string client_tag_hash;
  int64 response_version;
  base::Time ctime;
  base::Time mtime;
  std::string non_unique_name;
  bool deleted;
  sync_pb::EntitySpecifics specifics;
};

typedef std::vector<CommitRequestData> CommitRequestDataList;
typedef std::vector<CommitResponseData> CommitResponseDataList;
typedef std::vector<UpdateResponseData> UpdateResponseDataList;

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_NON_BLOCKING_SYNC_COMMON_H_
