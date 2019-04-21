// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/debug_info_collector.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "google_apis/drive/task_util.h"

namespace drive {

namespace {

void IterateFileCacheInternal(
    internal::ResourceMetadata* metadata,
    const DebugInfoCollector::IterateFileCacheCallback& iteration_callback) {
  std::unique_ptr<internal::ResourceMetadata::Iterator> it =
      metadata->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    if (it->GetValue().file_specific_info().has_cache_state()) {
      iteration_callback.Run(it->GetID(),
                             it->GetValue().file_specific_info().cache_state());
    }
  }
  DCHECK(!it->HasError());
}

// Runs the callback with arguments.
void RunGetResourceEntryCallback(GetResourceEntryCallback callback,
                                 std::unique_ptr<ResourceEntry> entry,
                                 FileError error) {
  DCHECK(callback);
  if (error != FILE_ERROR_OK)
    entry.reset();
  std::move(callback).Run(error, std::move(entry));
}

// Runs the callback with arguments.
void RunReadDirectoryCallback(
    const DebugInfoCollector::ReadDirectoryCallback& callback,
    std::unique_ptr<ResourceEntryVector> entries,
    FileError error) {
  DCHECK(callback);
  if (error != FILE_ERROR_OK)
    entries.reset();
  callback.Run(error, std::move(entries));
}

}  // namespace

DebugInfoCollector::DebugInfoCollector(
    internal::ResourceMetadata* metadata,
    FileSystemInterface* file_system,
    base::SequencedTaskRunner* blocking_task_runner)
    : metadata_(metadata),
      file_system_(file_system),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(metadata_);
  DCHECK(file_system_);
}

DebugInfoCollector::~DebugInfoCollector() = default;

void DebugInfoCollector::GetResourceEntry(const base::FilePath& file_path,
                                          GetResourceEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  std::unique_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&internal::ResourceMetadata::GetResourceEntryByPath,
                     base::Unretained(metadata_), file_path, entry_ptr),
      base::BindOnce(&RunGetResourceEntryCallback, std::move(callback),
                     base::Passed(&entry)));
}

void DebugInfoCollector::ReadDirectory(
    const base::FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  std::unique_ptr<ResourceEntryVector> entries(new ResourceEntryVector);
  ResourceEntryVector* entries_ptr = entries.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::ReadDirectoryByPath,
                 base::Unretained(metadata_),
                 file_path,
                 entries_ptr),
      base::Bind(&RunReadDirectoryCallback, callback, base::Passed(&entries)));
}

void DebugInfoCollector::IterateFileCache(
    const IterateFileCacheCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(iteration_callback);
  DCHECK(completion_callback);

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&IterateFileCacheInternal, metadata_,
                     google_apis::CreateRelayCallback(iteration_callback)),
      completion_callback);
}

void DebugInfoCollector::GetMetadata(GetFilesystemMetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  // Currently, this is just a proxy to the FileSystem.
  // TODO(hidehiko): Move the implementation to here to simplify the
  // FileSystem's implementation. crbug.com/237088
  file_system_->GetMetadata(std::move(callback));
}

}  // namespace drive
