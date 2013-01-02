// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#define SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/syncable/write_transaction_info.h"

namespace syncer {
namespace syncable {

class SYNC_EXPORT_PRIVATE TransactionObserver {
 public:
  virtual void OnTransactionWrite(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      ModelTypeSet models_with_changes) = 0;
 protected:
  virtual ~TransactionObserver() {}
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
