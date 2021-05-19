// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_change.h"

#include <ostream>

#include "base/notreached.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

SyncChange::SyncChange(const base::Location& from_here,
                       SyncChangeType change_type,
                       const SyncData& sync_data)
    : location_(from_here), change_type_(change_type), sync_data_(sync_data) {
  DCHECK(IsValid());
}

SyncChange::~SyncChange() {}

bool SyncChange::IsValid() const {
  // TODO(crbug.com/1152824): This implementation could be simplified if the
  // public API provides guarantees around when it returns false.
  if (!sync_data_.IsValid()) {
    return false;
  }

  if (!IsRealDataType(sync_data_.GetDataType())) {
    return false;
  }

  // Data from the syncer must always have valid specifics.
  if (!sync_data_.IsLocal()) {
    return true;
  }

  // Local changes must always have a unique tag.
  if (sync_data_.GetClientTagHash().value().empty()) {
    return false;
  }

  // Adds and updates must have a non-unique-title.
  if (change_type_ == ACTION_ADD || change_type_ == ACTION_UPDATE) {
    return !sync_data_.GetTitle().empty();
  }

  return true;
}

SyncChange::SyncChangeType SyncChange::change_type() const {
  return change_type_;
}

SyncData SyncChange::sync_data() const {
  return sync_data_;
}

base::Location SyncChange::location() const {
  return location_;
}

// static
std::string SyncChange::ChangeTypeToString(SyncChangeType change_type) {
  switch (change_type) {
    case ACTION_ADD:
      return "ACTION_ADD";
    case ACTION_UPDATE:
      return "ACTION_UPDATE";
    case ACTION_DELETE:
      return "ACTION_DELETE";
  }
}

std::string SyncChange::ToString() const {
  return "{ " + location_.ToString() + ", changeType: " +
         ChangeTypeToString(change_type_) + ", syncData: " +
         sync_data_.ToString() + "}";
}

void PrintTo(const SyncChange& sync_change, std::ostream* os) {
  *os << sync_change.ToString();
}

}  // namespace syncer
