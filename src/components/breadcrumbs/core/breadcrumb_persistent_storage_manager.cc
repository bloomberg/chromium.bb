// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"

#include <string.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"

namespace breadcrumbs {

namespace {

const char kEventSeparator[] = "\n";

// Minimum time between breadcrumb writes to disk.
constexpr auto kMinDelayBetweenWrites = base::Milliseconds(250);

// Writes |events| to |file_path| at |position|.
void DoInsertEventsIntoMemoryMappedFile(const base::FilePath& file_path,
                                        const size_t position,
                                        const std::string& events) {
  auto file = std::make_unique<base::MemoryMappedFile>();
  const base::MemoryMappedFile::Region region = {0, kPersistedFilesizeInBytes};
  const bool file_valid = file->Initialize(
      base::File(file_path, base::File::FLAG_OPEN_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE),
      region, base::MemoryMappedFile::READ_WRITE_EXTEND);

  if (file_valid) {
    char* data = reinterpret_cast<char*>(file->data());
    strcpy(&data[position], events.data());
  }
}

// Writes |events| to |file_path| overwriting any existing data.
void DoWriteEventsToFile(const base::FilePath& file_path,
                         const std::string& events) {
  const base::MemoryMappedFile::Region region = {0, kPersistedFilesizeInBytes};
  base::MemoryMappedFile file;
  const bool file_valid = file.Initialize(
      base::File(file_path, base::File::FLAG_CREATE_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE),
      region, base::MemoryMappedFile::READ_WRITE_EXTEND);

  if (file_valid) {
    char* data = reinterpret_cast<char*>(file.data());
    strcpy(data, events.data());
  }
}

void DoReplaceFile(const base::FilePath& from_path,
                   const base::FilePath& to_path) {
  base::ReplaceFile(from_path, to_path, nullptr);
}

// Returns breadcrumb events stored at |file_path|.
std::vector<std::string> DoGetStoredEvents(const base::FilePath& file_path) {
  base::File events_file(file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid()) {
    // File may not yet exist.
    return std::vector<std::string>();
  }

  size_t file_size = events_file.GetLength();
  if (file_size <= 0) {
    return std::vector<std::string>();
  }

  // Do not read more than |kPersistedFilesizeInBytes|, in case the file was
  // corrupted. If |kPersistedFilesizeInBytes| has been reduced since the last
  // breadcrumbs file was saved, this could result in a one time loss of the
  // oldest breadcrumbs which is ok because the decision has already been made
  // to reduce the size of the stored breadcrumbs.
  if (file_size > kPersistedFilesizeInBytes) {
    file_size = kPersistedFilesizeInBytes;
  }

  std::vector<uint8_t> data;
  data.resize(file_size);
  if (!events_file.ReadAndCheck(/*offset=*/0, data)) {
    return std::vector<std::string>();
  }
  const std::string persisted_events(data.begin(), data.end());
  const std::string all_events =
      persisted_events.substr(/*pos=*/0, strlen(persisted_events.c_str()));
  return base::SplitString(all_events, kEventSeparator, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

// Returns the total length of stored breadcrumb events at |file_path|. The
// file is opened and the length of the string contents calculated because
// the file size is always constant. (Due to base::MemoryMappedFile filling the
// unused space with \0s.
size_t DoGetStoredEventsLength(const base::FilePath& file_path) {
  base::File events_file(file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid()) {
    return 0;
  }

  size_t file_size = events_file.GetLength();
  if (file_size <= 0) {
    return 0;
  }

  // Do not read more than |kPersistedFilesizeInBytes|, in case the file was
  // corrupted. If |kPersistedFilesizeInBytes| has been reduced since the last
  // breadcrumbs file was saved, this could result in a one time loss of the
  // oldest breadcrumbs which is ok because the decision has already been made
  // to reduce the size of the stored breadcrumbs.
  if (file_size > kPersistedFilesizeInBytes) {
    file_size = kPersistedFilesizeInBytes;
  }

  std::vector<uint8_t> data;
  data.resize(file_size);
  if (!events_file.ReadAndCheck(/*offset=*/0, data)) {
    return 0;
  }

  const std::string persisted_events(data.begin(), data.end());
  return strlen(persisted_events.c_str());
}

// Renames breadcrumb files with the old filenames "iOS Breadcrumbs.*" to the
// new filenames "Breadcrumbs.*", if present.
void MigrateOldBreadcrumbFiles(
    const base::FilePath& old_breadcrumbs_file_path,
    const base::FilePath& old_breadcrumbs_temp_file_path,
    const base::FilePath& breadcrumbs_file_path,
    const base::FilePath& breadcrumbs_temp_file_path) {
  if (base::PathExists(old_breadcrumbs_file_path))
    base::Move(old_breadcrumbs_file_path, breadcrumbs_file_path);
  if (base::PathExists(old_breadcrumbs_temp_file_path))
    base::Move(old_breadcrumbs_temp_file_path, breadcrumbs_temp_file_path);
}

}  // namespace

BreadcrumbPersistentStorageManager::BreadcrumbPersistentStorageManager(
    const base::FilePath& directory,
    const absl::optional<base::FilePath>& old_breadcrumbs_file_path,
    const absl::optional<base::FilePath>& old_breadcrumbs_temp_file_path)
    :  // Ensure first event will not be delayed by initializing with a time in
       // the past.
      last_written_time_(base::TimeTicks::Now() - kMinDelayBetweenWrites),
      breadcrumbs_file_path_(GetBreadcrumbPersistentStorageFilePath(directory)),
      breadcrumbs_temp_file_path_(
          GetBreadcrumbPersistentStorageTempFilePath(directory)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      weak_ptr_factory_(this) {
  // Rename breadcrumb files using the old filenames, if present. This must
  // happen before the files are used, to ensure that old breadcrumbs are found.
  // TODO(crbug.com/1187988): remove this and its unit test.
  if (old_breadcrumbs_file_path && old_breadcrumbs_temp_file_path) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MigrateOldBreadcrumbFiles,
                       old_breadcrumbs_file_path.value(),
                       old_breadcrumbs_temp_file_path.value(),
                       breadcrumbs_file_path_, breadcrumbs_temp_file_path_));
  }
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DoGetStoredEventsLength, breadcrumbs_file_path_),
      base::BindOnce(
          &BreadcrumbPersistentStorageManager::SetCurrentMappedFilePosition,
          weak_ptr_factory_.GetWeakPtr()));
}

BreadcrumbPersistentStorageManager::~BreadcrumbPersistentStorageManager() =
    default;

void BreadcrumbPersistentStorageManager::GetStoredEvents(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetStoredEvents, breadcrumbs_file_path_),
      std::move(callback));
}

void BreadcrumbPersistentStorageManager::MonitorBreadcrumbManager(
    BreadcrumbManager* manager) {
  manager->AddObserver(this);
}

void BreadcrumbPersistentStorageManager::MonitorBreadcrumbManagerService(
    BreadcrumbManagerKeyedService* service) {
  service->AddObserver(this);
}

void BreadcrumbPersistentStorageManager::StopMonitoringBreadcrumbManager(
    BreadcrumbManager* manager) {
  manager->RemoveObserver(this);
}

void BreadcrumbPersistentStorageManager::StopMonitoringBreadcrumbManagerService(
    BreadcrumbManagerKeyedService* service) {
  service->RemoveObserver(this);
}

void BreadcrumbPersistentStorageManager::CombineEventsAndRewriteAllBreadcrumbs(
    const std::vector<std::string> pending_breadcrumbs,
    std::vector<std::string> existing_events) {
  // Add events which had not yet been written.
  for (const auto& event : pending_breadcrumbs) {
    existing_events.push_back(event);
  }

  std::vector<std::string> breadcrumbs;
  for (auto event_it = existing_events.rbegin();
       event_it != existing_events.rend(); ++event_it) {
    // Reduce saved events to only fill the amount which would be included on
    // a crash log. This allows future events to be appended individually up
    // to |kPersistedFilesizeInBytes|, which is more efficient than writing
    // out the
    const int event_with_seperator_size =
        event_it->size() + strlen(kEventSeparator);
    if (event_with_seperator_size + current_mapped_file_position_.value() >=
        kMaxDataLength) {
      break;
    }

    breadcrumbs.push_back(kEventSeparator);
    breadcrumbs.push_back(*event_it);
    current_mapped_file_position_ =
        current_mapped_file_position_.value() + event_with_seperator_size;
  }

  std::reverse(breadcrumbs.begin(), breadcrumbs.end());
  const std::string breadcrumbs_string = base::JoinString(breadcrumbs, "");

  task_runner_->PostTask(FROM_HERE, base::BindOnce(&DoWriteEventsToFile,
                                                   breadcrumbs_temp_file_path_,
                                                   breadcrumbs_string));

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DoReplaceFile, breadcrumbs_temp_file_path_,
                                breadcrumbs_file_path_));
}

void BreadcrumbPersistentStorageManager::RewriteAllExistingBreadcrumbs() {
  // Collect breadcrumbs which haven't been written yet to include in this full
  // re-write.
  std::vector<std::string> pending_breadcrumbs =
      base::SplitString(pending_breadcrumbs_, kEventSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  pending_breadcrumbs_.clear();
  write_timer_.Stop();

  last_written_time_ = base::TimeTicks::Now();
  current_mapped_file_position_ = 0;

  // Load persisted events directly from file because the correct order can not
  // be reconstructed from the multiple BreadcrumbManagers with the partial
  // timestamps embedded in each event.
  GetStoredEvents(base::BindOnce(&BreadcrumbPersistentStorageManager::
                                     CombineEventsAndRewriteAllBreadcrumbs,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 pending_breadcrumbs));
}

void BreadcrumbPersistentStorageManager::WritePendingBreadcrumbs() {
  if (pending_breadcrumbs_.empty()) {
    return;
  }

  // Make a copy of |pending_breadcrumbs_| to pass to the
  // DoInsertEventsIntoMemoryMappedFile() callback, since |pending_breadcrumbs_|
  // is about to be cleared.
  const std::string pending_breadcrumbs = pending_breadcrumbs_;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DoInsertEventsIntoMemoryMappedFile,
                                        breadcrumbs_file_path_,
                                        current_mapped_file_position_.value(),
                                        pending_breadcrumbs));

  current_mapped_file_position_ =
      current_mapped_file_position_.value() + pending_breadcrumbs_.size();
  last_written_time_ = base::TimeTicks::Now();

  pending_breadcrumbs_.clear();
}

void BreadcrumbPersistentStorageManager::EventAdded(BreadcrumbManager* manager,
                                                    const std::string& event) {
  WriteEvent(event);
}

void BreadcrumbPersistentStorageManager::WriteEvent(const std::string& event) {
  pending_breadcrumbs_ += event + kEventSeparator;

  WriteEvents();
}

void BreadcrumbPersistentStorageManager::SetCurrentMappedFilePosition(
    size_t file_size) {
  current_mapped_file_position_ = file_size;
}

void BreadcrumbPersistentStorageManager::WriteEvents() {
  write_timer_.Stop();

  const base::TimeDelta time_delta_since_last_write =
      base::TimeTicks::Now() - last_written_time_;
  // Delay writing the event to disk if an event was just written or if the size
  // of exisiting breadcrumbs is not yet known.
  if (time_delta_since_last_write < kMinDelayBetweenWrites ||
      !current_mapped_file_position_) {
    write_timer_.Start(FROM_HERE,
                       kMinDelayBetweenWrites - time_delta_since_last_write,
                       this, &BreadcrumbPersistentStorageManager::WriteEvents);
  } else {
    // If the event does not fit within |kPersistedFilesizeInBytes|, rewrite the
    // file to trim old events.
    if ((current_mapped_file_position_.value() + pending_breadcrumbs_.size())
        // Use >= here instead of > to allow space for \0 to terminate file.
        >= kPersistedFilesizeInBytes) {
      RewriteAllExistingBreadcrumbs();
      return;
    }

    // Otherwise, simply append the pending breadcrumbs.
    WritePendingBreadcrumbs();
  }
}

void BreadcrumbPersistentStorageManager::OldEventsRemoved(
    BreadcrumbManager* manager) {
  RewriteAllExistingBreadcrumbs();
}

}  // namespace breadcrumbs
