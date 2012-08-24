// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base_transaction.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/util/cryptographer.h"

namespace syncer {

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share) {
  DCHECK(share && share->directory.get());
  directory_ = share->directory.get();
}
BaseTransaction::~BaseTransaction() {
}

Cryptographer* BaseTransaction::GetCryptographer() const {
  return directory_->GetCryptographer(this->GetWrappedTrans());
}

ModelTypeSet BaseTransaction::GetEncryptedTypes() const {
  return directory_->GetNigoriHandler()->GetEncryptedTypes(
      this->GetWrappedTrans());
}

}  // namespace syncer
