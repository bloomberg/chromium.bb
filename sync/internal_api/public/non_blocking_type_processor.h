// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_NON_BLOCKING_TYPE_PROCESSOR_H_
#define SYNC_ENGINE_NON_BLOCKING_TYPE_PROCESSOR_H_

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

class SyncCoreProxy;
class NonBlockingTypeProcessorCore;

// A sync component embedded on the synced type's thread that helps to handle
// communication between sync and model type threads.
class SYNC_EXPORT_PRIVATE NonBlockingTypeProcessor : base::NonThreadSafe {
 public:
  NonBlockingTypeProcessor(ModelType type);
  virtual ~NonBlockingTypeProcessor();

  // Returns true if the handshake with sync thread is complete.
  bool IsEnabled() const;

  // Returns the model type handled by this processor.
  ModelType GetModelType() const;

  // Starts the handshake with the sync thread.
  void Enable(SyncCoreProxy* core_proxy);

  // Severs all ties to the sync thread.
  // Another call to Enable() can be used to re-establish this connection.
  void Disable();

  // Callback used to process the handshake response.
  void OnConnect(base::WeakPtr<NonBlockingTypeProcessorCore> core,
                 scoped_refptr<base::SequencedTaskRunner> sync_thread);

  base::WeakPtr<NonBlockingTypeProcessor> AsWeakPtr();

 private:
  ModelType type_;
  sync_pb::DataTypeProgressMarker progress_marker_;
  bool enabled_;

  base::WeakPtr<NonBlockingTypeProcessorCore> core_;
  scoped_refptr<base::SequencedTaskRunner> sync_thread_;

  base::WeakPtrFactory<NonBlockingTypeProcessor> weak_ptr_factory_;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_NON_BLOCKING_TYPE_PROCESSOR_H_
