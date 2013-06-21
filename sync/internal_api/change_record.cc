// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/change_record.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

ChangeRecord::ChangeRecord()
    : id(kInvalidId), action(ACTION_ADD) {}

ChangeRecord::~ChangeRecord() {}

base::DictionaryValue* ChangeRecord::ToValue() const {
  base::DictionaryValue* value = new base::DictionaryValue();
  std::string action_str;
  switch (action) {
    case ACTION_ADD:
      action_str = "Add";
      break;
    case ACTION_DELETE:
      action_str = "Delete";
      break;
    case ACTION_UPDATE:
      action_str = "Update";
      break;
    default:
      NOTREACHED();
      action_str = "Unknown";
      break;
  }
  value->SetString("action", action_str);
  value->SetString("id", base::Int64ToString(id));
  if (action == ACTION_DELETE) {
    if (extra.get()) {
      value->Set("extra", extra->ToValue());
    }
    value->Set("specifics", EntitySpecificsToValue(specifics));
  }
  return value;
}

ExtraPasswordChangeRecordData::ExtraPasswordChangeRecordData() {}

ExtraPasswordChangeRecordData::ExtraPasswordChangeRecordData(
    const sync_pb::PasswordSpecificsData& data)
    : unencrypted_(data) {
}

ExtraPasswordChangeRecordData::~ExtraPasswordChangeRecordData() {}

base::DictionaryValue* ExtraPasswordChangeRecordData::ToValue() const {
  return PasswordSpecificsDataToValue(unencrypted_);
}

const sync_pb::PasswordSpecificsData&
    ExtraPasswordChangeRecordData::unencrypted() const {
  return unencrypted_;
}

}  // namespace syncer
