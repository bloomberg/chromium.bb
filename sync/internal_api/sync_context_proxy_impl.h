// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_CONTEXT_PROXY_IMPL_H_
#define SYNC_INTERNAL_API_SYNC_CONTEXT_PROXY_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_context_proxy.h"

namespace syncer {

class SyncContext;
class ModelTypeSyncProxyImpl;
struct DataTypeState;

// Encapsulates a reference to the sync context and the thread it's running on.
// Used by sync's data types to connect with the sync context.
//
// It is expected that this object will be copied to and used on many different
// threads.  It is small and safe to pass by value.
class SYNC_EXPORT_PRIVATE SyncContextProxyImpl : public SyncContextProxy {
 public:
  SyncContextProxyImpl(
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner,
      const base::WeakPtr<SyncContext>& sync_context);
  virtual ~SyncContextProxyImpl();

  // Attempts to connect a non-blocking type to the sync context.
  //
  // This may fail under some unusual circumstances, like shutdown.  Due to the
  // nature of WeakPtrs and cross-thread communication, the caller will be
  // unable to distinguish a slow success from failure.
  //
  // Must be called from the thread where the data type lives.
  virtual void ConnectTypeToSync(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      const UpdateResponseDataList& pending_updates,
      const base::WeakPtr<ModelTypeSyncProxyImpl>& sync_proxy_impl) OVERRIDE;

  // Disables syncing for the given type on the sync thread.
  virtual void Disconnect(syncer::ModelType type) OVERRIDE;

  virtual scoped_ptr<SyncContextProxy> Clone() const OVERRIDE;

 private:
  // A SequencedTaskRunner representing the thread where the SyncContext lives.
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // The SyncContext this object is wrapping.
  base::WeakPtr<SyncContext> sync_context_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_CONTEXT_PROXY_IMPL_H_
