// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_PUBLIC_API_CHANGE_RECORD_H_
#define SYNC_INTERNAL_PUBLIC_API_CHANGE_RECORD_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/immutable.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {

// TODO(zea): One day get passwords playing nicely with the rest of encryption
// and get rid of this.
class SYNC_EXPORT ExtraPasswordChangeRecordData {
 public:
  ExtraPasswordChangeRecordData();
  explicit ExtraPasswordChangeRecordData(
      const sync_pb::PasswordSpecificsData& data);
  virtual ~ExtraPasswordChangeRecordData();

  // Transfers ownership of the DictionaryValue to the caller.
  virtual base::DictionaryValue* ToValue() const;

  const sync_pb::PasswordSpecificsData& unencrypted() const;
 private:
  sync_pb::PasswordSpecificsData unencrypted_;
};

// ChangeRecord indicates a single item that changed as a result of a sync
// operation.  This gives the sync id of the node that changed, and the type
// of change.  To get the actual property values after an ADD or UPDATE, the
// client should get the node with InitByIdLookup(), using the provided id.
struct SYNC_EXPORT_PRIVATE ChangeRecord {
  enum Action {
    ACTION_ADD,
    ACTION_DELETE,
    ACTION_UPDATE,
  };
  ChangeRecord();
  ~ChangeRecord();

  // Transfers ownership of the DictionaryValue to the caller.
  base::DictionaryValue* ToValue() const;

  int64 id;
  Action action;
  sync_pb::EntitySpecifics specifics;
  linked_ptr<ExtraPasswordChangeRecordData> extra;
};

typedef std::vector<ChangeRecord> ChangeRecordList;

typedef Immutable<ChangeRecordList> ImmutableChangeRecordList;

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_CHANGE_RECORD_H_
