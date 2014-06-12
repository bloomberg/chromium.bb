// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/injectable_sync_core_proxy.h"

#include "sync/engine/non_blocking_type_processor.h"
#include "sync/engine/non_blocking_type_processor_core_interface.h"

namespace syncer {

InjectableSyncCoreProxy::InjectableSyncCoreProxy(
    NonBlockingTypeProcessorCoreInterface* core)
    : is_core_connected_(false), processor_core_(core) {
}

InjectableSyncCoreProxy::~InjectableSyncCoreProxy() {
}

void InjectableSyncCoreProxy::ConnectTypeToCore(
    syncer::ModelType type,
    const DataTypeState& data_type_state,
    base::WeakPtr<syncer::NonBlockingTypeProcessor> type_processor) {
  // This class is allowed to participate in only one connection.
  DCHECK(!is_core_connected_);
  is_core_connected_ = true;

  // Hands off ownership of our member to the type_processor, while keeping
  // an unsafe pointer to it.  This is why we can only connect once.
  scoped_ptr<NonBlockingTypeProcessorCoreInterface> core(processor_core_);

  type_processor->OnConnect(core.Pass());
}

void InjectableSyncCoreProxy::Disconnect(syncer::ModelType type) {
  // This mock object is not meant for connect and disconnect tests.
  NOTREACHED() << "Not implemented";
}

scoped_ptr<SyncCoreProxy> InjectableSyncCoreProxy::Clone() const {
  // This confuses ownership.  We trust that our callers are well-behaved.
  return scoped_ptr<SyncCoreProxy>(
      new InjectableSyncCoreProxy(processor_core_));
}

NonBlockingTypeProcessorCoreInterface*
InjectableSyncCoreProxy::GetProcessorCore() {
  return processor_core_;
}

}  // namespace syncer
