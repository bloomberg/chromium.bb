// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/non_blocking_type_processor.h"

#include "base/message_loop/message_loop_proxy.h"
#include "sync/engine/non_blocking_type_processor_core.h"
#include "sync/internal_api/public/sync_core_proxy.h"

namespace syncer {

NonBlockingTypeProcessor::NonBlockingTypeProcessor(ModelType type)
  : type_(type),
    is_preferred_(false),
    is_connected_(false),
    weak_ptr_factory_(this) {}

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

void NonBlockingTypeProcessor::Enable(SyncCoreProxy* core_proxy) {
  DCHECK(CalledOnValidThread());
  is_preferred_ = true;
  core_proxy->ConnectTypeToCore(GetModelType(), AsWeakPtr());
}

void NonBlockingTypeProcessor::Disable() {
  DCHECK(CalledOnValidThread());
  is_preferred_ = false;
  Disconnect();
}

void NonBlockingTypeProcessor::Disconnect() {
  DCHECK(CalledOnValidThread());
  is_connected_ = false;
  core_ = base::WeakPtr<NonBlockingTypeProcessorCore>();
  sync_thread_ = scoped_refptr<base::SequencedTaskRunner>();
}

base::WeakPtr<NonBlockingTypeProcessor> NonBlockingTypeProcessor::AsWeakPtr() {
  DCHECK(CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void NonBlockingTypeProcessor::OnConnect(
    base::WeakPtr<NonBlockingTypeProcessorCore> core,
    scoped_refptr<base::SequencedTaskRunner> sync_thread) {
  DCHECK(CalledOnValidThread());
  is_connected_ = true;
  core_ = core;
  sync_thread_ = sync_thread;
}

}  // namespace syncer
