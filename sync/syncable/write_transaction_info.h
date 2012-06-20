// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_WRITE_TRANSACTION_INFO_H_
#define SYNC_SYNCABLE_WRITE_TRANSACTION_INFO_H_
#pragma once

#include "sync/syncable/base_transaction.h"
#include "sync/syncable/entry_kernel.h"

namespace syncable {

// A struct describing the changes made during a transaction.
struct WriteTransactionInfo {
  WriteTransactionInfo(int64 id,
                       tracked_objects::Location location,
                       WriterTag writer,
                       ImmutableEntryKernelMutationMap mutations);
  WriteTransactionInfo();
  ~WriteTransactionInfo();

  // Caller owns the return value.
  base::DictionaryValue* ToValue(size_t max_mutations_size) const;

  int64 id;
  // If tracked_objects::Location becomes assignable, we can use that
  // instead.
  std::string location_string;
  WriterTag writer;
  ImmutableEntryKernelMutationMap mutations;
};

typedef
    browser_sync::Immutable<WriteTransactionInfo>
    ImmutableWriteTransactionInfo;

}  // namespace syncable

#endif  // SYNC_SYNCABLE_WRITE_TRANSACTION_INFO_H_
