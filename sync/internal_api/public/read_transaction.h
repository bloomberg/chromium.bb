// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_READ_TRANSACTION_H_
#define SYNC_INTERNAL_API_PUBLIC_READ_TRANSACTION_H_

#include "base/compiler_specific.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base_transaction.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace sync_pb {
class DataTypeContext;
}

namespace syncer {

struct UserShare;

// Sync API's ReadTransaction is a read-only BaseTransaction.  It wraps
// a syncable::ReadTransaction.
class SYNC_EXPORT ReadTransaction : public BaseTransaction {
 public:
  // Start a new read-only transaction on the specified repository.
  ReadTransaction(const tracked_objects::Location& from_here,
                  UserShare* share);

  // Resume the middle of a transaction. Will not close transaction.
  ReadTransaction(UserShare* share, syncable::BaseTransaction* trans);

  virtual ~ReadTransaction();

  // BaseTransaction override.
  virtual syncable::BaseTransaction* GetWrappedTrans() const OVERRIDE;

  // Return |transaction_version| of |type| stored in sync directory's
  // persisted info.
  int64 GetModelVersion(ModelType type) const;

  // Fills |context| with the datatype context associated with |type|.
  void GetDataTypeContext(ModelType type,
                          sync_pb::DataTypeContext* context) const;

  // Clears |id_set| and fills it with the ids of attachments that need to be
  // uploaded to the sync server.
  void GetAttachmentIdsToUpload(ModelType type, AttachmentIdSet* id_set);

 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::BaseTransaction* transaction_;
  bool close_transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_READ_TRANSACTION_H_
