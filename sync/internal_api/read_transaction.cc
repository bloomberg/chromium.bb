// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/read_transaction.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_read_transaction.h"

namespace syncer {

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

int64 ReadTransaction::GetModelVersion(ModelType type) const {
  return transaction_->directory()->GetTransactionVersion(type);
}

void ReadTransaction::GetDataTypeContext(
    ModelType type,
    sync_pb::DataTypeContext* context) const {
  return transaction_->directory()->GetDataTypeContext(
      transaction_, type, context);
}

void ReadTransaction::GetAttachmentIdsToUpload(ModelType type,
                                               AttachmentIdSet* id_set) {
  DCHECK(id_set);
  transaction_->directory()->GetAttachmentIdsToUpload(
      transaction_, type, id_set);
}

}  // namespace syncer
