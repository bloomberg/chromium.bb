// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/non_blocking_type_processor.h"

#include "base/message_loop/message_loop_proxy.h"
#include "sync/engine/non_blocking_type_processor_core.h"
#include "sync/internal_api/public/sync_core_proxy.h"

namespace syncer {

NonBlockingTypeProcessor::NonBlockingTypeProcessor(ModelType type)
    : type_(type),
      is_preferred_(false),
      is_connected_(false),
      weak_ptr_factory_for_ui_(this),
      weak_ptr_factory_for_sync_(this) {
}

NonBlockingTypeProcessor::~NonBlockingTypeProcessor() {
}

bool NonBlockingTypeProcessor::IsPreferred() const {
  DCHECK(CalledOnValidThread());
  return is_preferred_;
}

bool NonBlockingTypeProcessor::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return is_connected_;
}

ModelType NonBlockingTypeProcessor::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

void NonBlockingTypeProcessor::Enable(
    scoped_ptr<SyncCoreProxy> sync_core_proxy) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Asked to enable " << ModelTypeToString(type_);
  is_preferred_ = true;
  sync_core_proxy_ = sync_core_proxy.Pass();
  sync_core_proxy_->ConnectTypeToCore(GetModelType(),
                                      weak_ptr_factory_for_sync_.GetWeakPtr());
}

void NonBlockingTypeProcessor::Disable() {
  DCHECK(CalledOnValidThread());
  is_preferred_ = false;
  Disconnect();
}

void NonBlockingTypeProcessor::Disconnect() {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Asked to disconnect " << ModelTypeToString(type_);
  is_connected_ = false;

  if (sync_core_proxy_) {
    sync_core_proxy_->Disconnect(GetModelType());
    sync_core_proxy_.reset();
  }

  weak_ptr_factory_for_sync_.InvalidateWeakPtrs();
  core_ = base::WeakPtr<NonBlockingTypeProcessorCore>();
  sync_thread_ = scoped_refptr<base::SequencedTaskRunner>();
}

base::WeakPtr<NonBlockingTypeProcessor>
NonBlockingTypeProcessor::AsWeakPtrForUI() {
  DCHECK(CalledOnValidThread());
  return weak_ptr_factory_for_ui_.GetWeakPtr();
}

void NonBlockingTypeProcessor::OnConnect(
    base::WeakPtr<NonBlockingTypeProcessorCore> core,
    scoped_refptr<base::SequencedTaskRunner> sync_thread) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Successfully connected " << ModelTypeToString(type_);
  is_connected_ = true;
  core_ = core;
  sync_thread_ = sync_thread;
}

}  // namespace syncer
