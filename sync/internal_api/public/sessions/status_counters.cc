// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/status_counters.h"

#include "base/json/json_string_value_serializer.h"
#include "base/values.h"

namespace syncer {

StatusCounters::StatusCounters()
  : num_entries(0),
    num_entries_and_tombstones(0) {}

StatusCounters::~StatusCounters() {}

scoped_ptr<base::DictionaryValue> StatusCounters::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("numEntries", num_entries);
  value->SetInteger("numEntriesAndTombstones", num_entries_and_tombstones);
  return value.Pass();
}

std::string StatusCounters::ToString() const {
  std::string result;
  scoped_ptr<base::DictionaryValue> value = ToValue();
  JSONStringValueSerializer serializer(&result);
  serializer.Serialize(*value);
  return result;
}

}  // namespace syncer
