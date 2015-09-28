// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer_v2 {
struct ActivationContext;

// Interface for the datatype integration logic from non-sync threads.
//
// See SyncContextProxyImpl for an actual implementation.
class SYNC_EXPORT_PRIVATE SyncContextProxy {
 public:
  SyncContextProxy();
  virtual ~SyncContextProxy();

  // Attempts to connect a non-blocking type to the sync context.
  virtual void ConnectTypeToSync(
      syncer::ModelType type,
      scoped_ptr<ActivationContext> activation_context) = 0;

  // Tells the syncer that we're no longer interested in syncing this type.
  //
  // Once this takes effect, the syncer can assume that it will no longer
  // receive commit requests for this type.  It should also stop requesting
  // and applying updates for this type, too.
  virtual void Disconnect(syncer::ModelType type) = 0;

  // Creates a clone of this SyncContextProxy.
  virtual scoped_ptr<SyncContextProxy> Clone() const = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_CONTEXT_PROXY_H_
