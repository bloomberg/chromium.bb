// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace breadcrumbs {

class BreadcrumbManager;
class BreadcrumbManagerKeyedService;

// The filesize for the file at |breadcrumbs_file_path_|. The file will always
// be this constant size because it is accessed using a memory mapped file. The
// file is twice as large as |kMaxDataLength| which leaves room for appending
// breadcrumb events. Once the file is full of events, the contents will be
// reduced to kMaxDataLength.
constexpr size_t kPersistedFilesizeInBytes = kMaxDataLength * 2;

// Stores breadcrumb events to and retrieves them from a file on disk.
// Persisting these events allows access to breadcrumb events from previous
// application sessions.
class BreadcrumbPersistentStorageManager : public BreadcrumbManagerObserver {
 public:
  // Breadcrumbs will be stored in a file in |directory|. If
  // |old_breadcrumbs_file_path| and |old_breadcrumbs_temp_file_path| are
  // provided, the files at those paths will be migrated to the new filenames
  // for breadcrumb files (only needed on iOS, which previously used different
  // filenames).
  explicit BreadcrumbPersistentStorageManager(
      const base::FilePath& directory,
      const absl::optional<base::FilePath>& old_breadcrumbs_file_path =
          absl::nullopt,
      const absl::optional<base::FilePath>& old_breadcrumbs_temp_file_path =
          absl::nullopt);
  ~BreadcrumbPersistentStorageManager() override;
  BreadcrumbPersistentStorageManager(
      const BreadcrumbPersistentStorageManager&) = delete;
  BreadcrumbPersistentStorageManager& operator=(
      const BreadcrumbPersistentStorageManager&) = delete;

  // Returns the stored breadcrumb events from disk to |callback|.
  void GetStoredEvents(
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Starts observing |manager| for events. Existing events will be persisted
  // immediately.
  void MonitorBreadcrumbManager(BreadcrumbManager* manager);
  // Starts observing |service| for events. Existing events will be persisted
  // immediately.
  void MonitorBreadcrumbManagerService(BreadcrumbManagerKeyedService* service);

  // Stops observing |manager|.
  void StopMonitoringBreadcrumbManager(BreadcrumbManager* manager);
  // Stops observing |service|.
  void StopMonitoringBreadcrumbManagerService(
      BreadcrumbManagerKeyedService* service);

 private:
  // Sets |current_mapped_file_position_| to |file_size|;
  void SetCurrentMappedFilePosition(size_t file_size);

  // Writes |pending_breadcrumbs_| to |breadcrumbs_file_| if it fits, otherwise
  // rewrites the file. NOTE: Writing may be delayed if the file has recently
  // been written into.
  void WriteEvents();

  // Appends events in |pending_breadcrumbs| to |existing events|, then writes
  // the combined events to |breadcrumbs_file_|, overwriting any existing
  // persisted breadcrumbs.
  void CombineEventsAndRewriteAllBreadcrumbs(
      const std::vector<std::string> pending_breadcrumbs,
      std::vector<std::string> existing_events);

  // Writes events from observed managers to |breadcrumbs_file_|, overwriting
  // any existing persisted breadcrumbs.
  void RewriteAllExistingBreadcrumbs();

  // Writes breadcrumbs stored in |pending_breadcrumbs_| to |breadcrumbs_file_|.
  void WritePendingBreadcrumbs();

  // Writes |event| to |breadcrumbs_file_|.
  // NOTE: Writing may be delayed if the file has recently been written into.
  void WriteEvent(const std::string& event);

  // BreadcrumbManagerObserver
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override;
  void OldEventsRemoved(BreadcrumbManager* manager) override;

  // Individual breadcrumbs that have not yet been written to disk.
  std::string pending_breadcrumbs_;

  // The last time a breadcrumb was written to |breadcrumbs_file_|. This
  // timestamp prevents breadcrumbs from being written to disk too often.
  base::TimeTicks last_written_time_;

  // A timer to delay writing to disk too often.
  base::OneShotTimer write_timer_;

  // The path to the file for storing persisted breadcrumbs.
  const base::FilePath breadcrumbs_file_path_;

  // The path to the temporary file for writing persisted breadcrumbs.
  const base::FilePath breadcrumbs_temp_file_path_;

  // The current size of breadcrumbs written to |breadcrumbs_file_path_|.
  // NOTE: The optional will not have a value until the size of the existing
  // file, if any, is retrieved.
  absl::optional<size_t> current_mapped_file_position_;

  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<BreadcrumbPersistentStorageManager> weak_ptr_factory_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
