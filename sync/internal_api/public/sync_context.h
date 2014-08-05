// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class ModelTypeSyncProxyImpl;
struct DataTypeState;

// An interface of the core parts of sync.
//
// In theory, this is the component that provides off-thread sync types with
// functionality to schedule and execute communication with the sync server.  In
// practice, this class delegates most of the responsibilty of implemeting this
// functionality to other classes, and most of the interface is exposed not
// directly here but instead through a per-ModelType class that this class helps
// instantiate.
class SYNC_EXPORT_PRIVATE SyncContext {
 public:
  SyncContext();
  virtual ~SyncContext();

  // Initializes the connection between the sync context on the sync thread and
  // a proxy for the specified non-blocking sync type that lives on the data
  // type's thread.
  virtual void ConnectSyncTypeToWorker(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      const scoped_refptr<base::SequencedTaskRunner>& datatype_task_runner,
      const base::WeakPtr<ModelTypeSyncProxyImpl>& type_sync_proxy) = 0;

  // Disconnects the syncer from the model and stops syncing the type.
  //
  // By the time this is called, the model thread should have already
  // invalidated the WeakPtr it sent to us in the connect request.  Any
  // messages sent to that ModelTypeSyncProxy will not be recived.
  //
  // This is the sync thread's chance to clear state associated with the type.
  // It also causes the syncer to stop requesting updates for this type, and to
  // abort any in-progress commit requests.
  virtual void DisconnectSyncWorker(ModelType type) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_H_
