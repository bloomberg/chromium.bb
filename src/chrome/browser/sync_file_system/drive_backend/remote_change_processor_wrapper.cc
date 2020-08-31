// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_wrapper.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"

namespace sync_file_system {
namespace drive_backend {

RemoteChangeProcessorWrapper::RemoteChangeProcessorWrapper(
    RemoteChangeProcessor* remote_change_processor)
    : remote_change_processor_(remote_change_processor) {}

void RemoteChangeProcessorWrapper::PrepareForProcessRemoteChange(
    const storage::FileSystemURL& url,
    const RemoteChangeProcessor::PrepareChangeCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  remote_change_processor_->PrepareForProcessRemoteChange(url, callback);
}

void RemoteChangeProcessorWrapper::ApplyRemoteChange(
    const FileChange& change,
    const base::FilePath& local_path,
    const storage::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  remote_change_processor_->ApplyRemoteChange(
      change, local_path, url,  callback);
}

void RemoteChangeProcessorWrapper::FinalizeRemoteSync(
    const storage::FileSystemURL& url,
    bool clear_local_changes,
    const base::Closure& completion_callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  remote_change_processor_->FinalizeRemoteSync(
    url, clear_local_changes, completion_callback);
}

void RemoteChangeProcessorWrapper::RecordFakeLocalChange(
    const storage::FileSystemURL& url,
    const FileChange& change,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  remote_change_processor_->RecordFakeLocalChange(url, change, callback);
}

}  // namespace drive_backend
}  // namespace sync_file_system
