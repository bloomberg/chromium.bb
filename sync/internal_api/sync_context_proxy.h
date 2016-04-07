// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_CONTEXT_PROXY_H_
#define SYNC_INTERNAL_API_SYNC_CONTEXT_PROXY_H_

#include "sync/internal_api/public/sync_context.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer_v2 {

// Encapsulates a reference to the sync context and the thread it's running on.
// Used by sync's data types to connect with the sync context.
class SYNC_EXPORT SyncContextProxy : public SyncContext {
 public:
  SyncContextProxy(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   const base::WeakPtr<SyncContext>& sync_context);
  ~SyncContextProxy() override;

  // SyncContext implementation
  void ConnectType(syncer::ModelType type,
                   scoped_ptr<ActivationContext> activation_context) override;
  void DisconnectType(syncer::ModelType type) override;

 private:
  // A SequencedTaskRunner representing the thread where the SyncContext lives.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The SyncContext this object is wrapping.
  base::WeakPtr<SyncContext> sync_context_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_SYNC_CONTEXT_PROXY_H_
