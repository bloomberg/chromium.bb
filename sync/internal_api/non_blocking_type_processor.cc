// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/non_blocking_type_processor.h"

#include "base/message_loop/message_loop_proxy.h"
#include "sync/engine/non_blocking_type_processor_core.h"
#include "sync/internal_api/public/sync_core_proxy.h"

namespace syncer {

NonBlockingTypeProcessor::NonBlockingTypeProcessor(ModelType type)
  : type_(type), enabled_(false), weak_ptr_factory_(this) {}

NonBlockingTypeProcessor::~NonBlockingTypeProcessor() {
}

bool NonBlockingTypeProcessor::IsEnabled() const {
  DCHECK(CalledOnValidThread());
  return enabled_;
}

ModelType NonBlockingTypeProcessor::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

void NonBlockingTypeProcessor::Enable(SyncCoreProxy* core_proxy) {
  DCHECK(CalledOnValidThread());
  core_proxy->ConnectTypeToCore(
      GetModelType(),
      AsWeakPtr());
}

void NonBlockingTypeProcessor::Disable() {
  DCHECK(CalledOnValidThread());
  enabled_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
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
  enabled_ = true;
  core_ = core;
  sync_thread_ = sync_thread;
}

}  // namespace syncer
