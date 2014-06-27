// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/write_transaction_info.h"

#include "base/strings/string_number_conversions.h"

namespace syncer {
namespace syncable {

WriteTransactionInfo::WriteTransactionInfo(
    int64 id,
    tracked_objects::Location location,
    WriterTag writer,
    ImmutableEntryKernelMutationMap mutations)
    : id(id),
      location_string(location.ToString()),
      writer(writer),
      mutations(mutations) {}

WriteTransactionInfo::WriteTransactionInfo()
    : id(-1), writer(INVALID) {}

WriteTransactionInfo::~WriteTransactionInfo() {}

base::DictionaryValue* WriteTransactionInfo::ToValue(
    size_t max_mutations_size) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("id", base::Int64ToString(id));
  dict->SetString("location", location_string);
  dict->SetString("writer", WriterTagToString(writer));
  base::Value* mutations_value = NULL;
  const size_t mutations_size = mutations.Get().size();
  if (mutations_size <= max_mutations_size) {
    mutations_value = EntryKernelMutationMapToValue(mutations.Get());
  } else {
    mutations_value =
        new base::StringValue(
            base::SizeTToString(mutations_size) + " mutations");
  }
  dict->Set("mutations", mutations_value);
  return dict;
}

}  // namespace syncable
}  // namespace syncer
