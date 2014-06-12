// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_INJECTABLE_SYNC_CORE_PROXY_H_
#define SYNC_TEST_ENGINE_INJECTABLE_SYNC_CORE_PROXY_H_

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_core_proxy.h"

namespace syncer {

struct DataTypeState;
class NonBlockingTypeProcessor;
class NonBlockingTypeProcessorCoreInterface;

// A SyncCoreProxy implementation that, when a connection request is made,
// initalizes a connection to a previously injected NonBlockingTypeProcessor.
class InjectableSyncCoreProxy : public syncer::SyncCoreProxy {
 public:
  explicit InjectableSyncCoreProxy(NonBlockingTypeProcessorCoreInterface* core);
  virtual ~InjectableSyncCoreProxy();

  virtual void ConnectTypeToCore(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      base::WeakPtr<syncer::NonBlockingTypeProcessor> type_processor) OVERRIDE;
  virtual void Disconnect(syncer::ModelType type) OVERRIDE;
  virtual scoped_ptr<SyncCoreProxy> Clone() const OVERRIDE;

  NonBlockingTypeProcessorCoreInterface* GetProcessorCore();

 private:
  // A flag to ensure ConnectTypeToCore is called at most once.
  bool is_core_connected_;

  // The NonBlockingTypeProcessor's contract expects that it gets to own this
  // object, so we can retain only a non-owned pointer to it.
  //
  // This is very unsafe, but we can get away with it since these tests are not
  // exercising the processor <-> processor_core connection code.
  NonBlockingTypeProcessorCoreInterface* processor_core_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_INJECTABLE_SYNC_CORE_PROXY_H_
