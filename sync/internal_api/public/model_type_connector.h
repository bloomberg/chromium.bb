// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_CONNECTOR_H_
#define SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_CONNECTOR_H_

#include <memory>

#include "sync/internal_api/public/base/model_type.h"

namespace syncer_v2 {
struct ActivationContext;

// An interface into the core parts of sync for USS model types. Handles
// creating the connection between the ModelTypeWorker (CommitQueue) on the sync
// side and the (Shared)ModelTypeProcessor on the model type side.
class SYNC_EXPORT ModelTypeConnector {
 public:
  ModelTypeConnector();
  virtual ~ModelTypeConnector();

  // Connect a worker on the sync thread and |type|'s processor on the model
  // thread. Note that in production |activation_context| actually owns a
  // processor proxy that forwards calls to the model thread and is safe to call
  // from the sync thread.
  virtual void ConnectType(
      syncer::ModelType type,
      std::unique_ptr<ActivationContext> activation_context) = 0;

  // Disconnects the worker from |type|'s processor and stop syncing the type.
  //
  // This is the sync thread's chance to clear state associated with the type.
  // It also causes the syncer to stop requesting updates for this type, and to
  // abort any in-progress commit requests.
  virtual void DisconnectType(syncer::ModelType type) = 0;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_CONNECTOR_H_
