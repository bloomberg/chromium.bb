// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class SyncCore;
class NonBlockingTypeProcessor;

// Encapsulates a reference to the sync core and the thread it's running on.
// Used by sync's data types to connect with the sync core.
//
// It is epxected that this object will be copied to and used on many different
// threads.  It is small and safe to pass by value.
class SYNC_EXPORT_PRIVATE SyncCoreProxy {
 public:
  SyncCoreProxy(
      scoped_refptr<base::SequencedTaskRunner> sync_task_runner,
      base::WeakPtr<SyncCore> sync_core);
  ~SyncCoreProxy();

  // Attempts to connect a non-blocking type to the sync core.
  //
  // This may fail under some unusual circumstances, like shutdown.  Due to the
  // nature of WeakPtrs and cross-thread communication, the caller will be
  // unable to distinguish a slow success from failure.
  //
  // Must be called from the thread where the data type lives.
  void ConnectTypeToCore(
      syncer::ModelType type,
      base::WeakPtr<NonBlockingTypeProcessor> type_processor);

  // Constructs and returns a useless instance of this object.
  static SyncCoreProxy GetInvalidSyncCoreProxyForTest();

 private:
  // A SequencedTaskRunner representing the thread where the SyncCore lives.
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // The SyncCore this object is wrapping.
  base::WeakPtr<SyncCore> sync_core_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_PROXY_H_
