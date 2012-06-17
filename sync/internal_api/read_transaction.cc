// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/read_transaction.h"

#include "sync/syncable/syncable.h"

namespace sync_api {

//////////////////////////////////////////////////////////////////////////
// ReadTransaction member definitions
ReadTransaction::ReadTransaction(const tracked_objects::Location& from_here,
                                 UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL),
      close_transaction_(true) {
  transaction_ = new syncable::ReadTransaction(from_here,
                                               share->directory.get());
}

ReadTransaction::ReadTransaction(UserShare* share,
                                 syncable::BaseTransaction* trans)
    : BaseTransaction(share),
      transaction_(trans),
      close_transaction_(false) {}

ReadTransaction::~ReadTransaction() {
  if (close_transaction_) {
    delete transaction_;
  }
}

syncable::BaseTransaction* ReadTransaction::GetWrappedTrans() const {
  return transaction_;
}

} // namespace sync_api
