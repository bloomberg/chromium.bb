// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#define SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#pragma once

#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/syncable/syncable.h"

namespace syncable {

class TransactionObserver {
 public:
  virtual void OnTransactionWrite(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      ModelTypeSet models_with_changes) = 0;
 protected:
  virtual ~TransactionObserver() {}
};

}  // namespace syncable

#endif  // SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
