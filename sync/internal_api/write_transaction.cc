// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/write_transaction.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_write_transaction.h"

namespace syncer {

//////////////////////////////////////////////////////////////////////////
// WriteTransaction member definitions
WriteTransaction::WriteTransaction(const tracked_objects::Location& from_here,
                                   UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::WriteTransaction(from_here, syncable::SYNCAPI,
                                                share->directory.get());
}

WriteTransaction::WriteTransaction(const tracked_objects::Location& from_here,
                                   UserShare* share,
                                   int64* new_model_version)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::WriteTransaction(from_here,
                                                share->directory.get(),
                                                new_model_version);
}

WriteTransaction::~WriteTransaction() {
  delete transaction_;
}

syncable::BaseTransaction* WriteTransaction::GetWrappedTrans() const {
  return transaction_;
}

void WriteTransaction::SetDataTypeContext(ModelType type,
                                          const std::string& context) {
  sync_pb::DataTypeContext local_context;
  GetDirectory()->GetDataTypeContext(transaction_,
                                     type,
                                     &local_context);
  if (local_context.context() == context)
    return;

  if (!local_context.has_data_type_id()) {
    local_context.set_data_type_id(
        syncer::GetSpecificsFieldNumberFromModelType(type));
  }
  DCHECK_EQ(syncer::GetSpecificsFieldNumberFromModelType(type),
            local_context.data_type_id());
  DCHECK_GE(local_context.version(), 0);
  local_context.set_version(local_context.version() + 1);
  local_context.set_context(context);
  GetDirectory()->SetDataTypeContext(transaction_,
                                     type,
                                     local_context);
}

}  // namespace syncer
