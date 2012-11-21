// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_TEST_TRANSACTION_OBSERVER_H_
#define SYNC_TEST_TEST_TRANSACTION_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/syncable/transaction_observer.h"
#include "sync/syncable/write_transaction_info.h"

namespace syncer {
namespace syncable {

// This class acts as a TransactionObserver for the syncable::Directory.
// It gathers information that is useful for writing test assertions.
class TestTransactionObserver :
  public base::SupportsWeakPtr<TestTransactionObserver>,
  public TransactionObserver {
 public:
  TestTransactionObserver();
  virtual ~TestTransactionObserver();
  virtual void OnTransactionWrite(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      ModelTypeSet models_with_changes) OVERRIDE;

  // Returns the number of transactions observed.
  //
  // Transactions are generated only when meaningful changes are made.  For most
  // testing purposes, you may assume that this counts the number of syncer
  // nudges generated.
  int transactions_observed();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTransactionObserver);

  int transactions_observed_;
};

}  // namespace syncable
}  // namespace syncer


#endif  // SYNC_TEST_TEST_TRANSACTION_OBSERVER_H_
