// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_TRANSACTION_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_TRANSACTION_H_
#pragma once

#include "sync/internal_api/public/user_share.h"

#include "sync/util/cryptographer.h"

namespace syncable {
class BaseTransaction;
class Directory;
}

namespace sync_api {

// Sync API's BaseTransaction, ReadTransaction, and WriteTransaction allow for
// batching of several read and/or write operations.  The read and write
// operations are performed by creating ReadNode and WriteNode instances using
// the transaction. These transaction classes wrap identically named classes in
// syncable, and are used in a similar way. Unlike syncable::BaseTransaction,
// whose construction requires an explicit syncable::Directory, a sync
// API BaseTransaction is created from a UserShare object.
class BaseTransaction {
 public:
  // Provide access to the underlying syncable.h objects from BaseNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const = 0;
  browser_sync::Cryptographer* GetCryptographer() const;

  syncable::Directory* GetDirectory() const {
    return directory_;
  }

 protected:
  explicit BaseTransaction(UserShare* share);
  virtual ~BaseTransaction();

  BaseTransaction() : directory_(NULL) { }

 private:
  syncable::Directory* directory_;

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

syncable::ModelTypeSet GetEncryptedTypes(
    const sync_api::BaseTransaction* trans);

}  // namespace sync_api

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_TRANSACTION_H_
