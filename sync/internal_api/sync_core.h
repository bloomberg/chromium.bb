// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class ModelTypeRegistry;
class NonBlockingTypeProcessor;
struct DataTypeState;

// An interface of the core parts of sync.
//
// In theory, this is the component that provides off-thread sync types with
// functionality to schedule and execute communication with the sync server.  In
// practice, this class delegates most of the responsibilty of implemeting this
// functionality to other classes, and most of the interface is exposed not
// directly here but instead through a per-ModelType class that this class helps
// instantiate.
class SYNC_EXPORT_PRIVATE SyncCore {
 public:
  explicit SyncCore(ModelTypeRegistry* model_type_registry);
  ~SyncCore();

  // Initializes the connection between the sync core and its delegate on the
  // sync client's thread.
  void ConnectSyncTypeToCore(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      scoped_refptr<base::SequencedTaskRunner> datatype_task_runner,
      base::WeakPtr<NonBlockingTypeProcessor> sync_client);

  // Disconnects the syncer from the model and stops syncing the type.
  //
  // By the time this is called, the model thread should have already
  // invalidated the WeakPtr it sent to us in the connect request.  Any
  // messages sent to that NonBlockingTypeProcessor will not be recived.
  //
  // This is the sync thread's chance to clear state associated with the type.
  // It also causes the syncer to stop requesting updates for this type, and to
  // abort any in-progress commit requests.
  void Disconnect(ModelType type);

  base::WeakPtr<SyncCore> AsWeakPtr();

 private:
  ModelTypeRegistry* model_type_registry_;
  base::WeakPtrFactory<SyncCore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncCore);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_CORE_H_

