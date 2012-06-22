// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_READ_TRANSACTION_H_
#define SYNC_SYNCABLE_READ_TRANSACTION_H_
#pragma once

#include "sync/syncable/base_transaction.h"

namespace csync {
class ReadTransaction;
}

namespace syncable {

// Locks db in constructor, unlocks in destructor.
class ReadTransaction : public BaseTransaction {
 public:
  ReadTransaction(const tracked_objects::Location& from_here,
                  Directory* directory);

  virtual ~ReadTransaction();

 protected:  // Don't allow creation on heap, except by sync API wrapper.
  friend class csync::ReadTransaction;
  void* operator new(size_t size) { return (::operator new)(size); }

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

}

#endif  // SYNC_SYNCABLE_READ_TRANSACTION_H_
