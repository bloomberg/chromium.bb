// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/non_blocking_type_processor_core.h"

#include "base/logging.h"
#include "sync/engine/commit_contribution.h"

namespace syncer {

NonBlockingTypeProcessorCore::NonBlockingTypeProcessorCore(
      ModelType type,
      scoped_refptr<base::SequencedTaskRunner> processor_task_runner,
      base::WeakPtr<NonBlockingTypeProcessor> processor)
    : type_(type),
      processor_task_runner_(processor_task_runner),
      processor_(processor),
      weak_ptr_factory_(this) {
  progress_marker_.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
}

NonBlockingTypeProcessorCore::~NonBlockingTypeProcessorCore() {
}

ModelType NonBlockingTypeProcessorCore::GetModelType() const {
  DCHECK(CalledOnValidThread());
  return type_;
}

// UpdateHandler implementation.
void NonBlockingTypeProcessorCore::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  DCHECK(CalledOnValidThread());
  // TODO(rlarocque): Implement this properly.  crbug.com/351005.
  DVLOG(1) << "Getting progress for: " << ModelTypeToString(type_);
  *progress_marker = progress_marker_;
}

void NonBlockingTypeProcessorCore::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  // TODO(rlarocque): Implement this properly.  crbug.com/351005.
  DVLOG(1) << "Getting context for: " << ModelTypeToString(type_);
  context->Clear();
}

SyncerError NonBlockingTypeProcessorCore::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());
  // TODO(rlarocque): Implement this properly.  crbug.com/351005.
  DVLOG(1) << "Processing updates response for: " << ModelTypeToString(type_);
  progress_marker_ = progress_marker;
  return SYNCER_OK;
}

void NonBlockingTypeProcessorCore::ApplyUpdates(
    sessions::StatusController* status) {
  DCHECK(CalledOnValidThread());
  // TODO(rlarocque): Implement this properly.  crbug.com/351005.
  DVLOG(1) << "Applying updates for: " << ModelTypeToString(type_);
}

void NonBlockingTypeProcessorCore::PassiveApplyUpdates(
    sessions::StatusController* status) {
  NOTREACHED()
      << "Non-blocking types should never apply updates on sync thread.  "
      << "ModelType is: " << ModelTypeToString(type_);
}

// CommitContributor implementation.
scoped_ptr<CommitContribution>
NonBlockingTypeProcessorCore::GetContribution(size_t max_entries) {
  DCHECK(CalledOnValidThread());
  // TODO(rlarocque): Implement this properly.  crbug.com/351005.
  DVLOG(1) << "Getting commit contribution for: " << ModelTypeToString(type_);
  return scoped_ptr<CommitContribution>();
}

base::WeakPtr<NonBlockingTypeProcessorCore>
NonBlockingTypeProcessorCore::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace syncer
