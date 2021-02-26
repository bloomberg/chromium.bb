// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"
#include "ios/chrome/browser/crash_report/crash_reporter_breadcrumb_constants.h"

// The filesize for the file at |breadcrumbs_file_path_|. The file will always
// be this constant size because it is accessed using a memory mapped file. The
// file is twice as large as |kMaxBreadcrumbsDataLength| which leaves room for
// appending breadcrumb events. Once the file is full of events, the contents
// will be reduced to kMaxBreadcrumbsDataLength.
constexpr size_t kPersistedFilesizeInBytes = kMaxBreadcrumbsDataLength * 2;

namespace base {
class FilePath;
}  // namespace base

class BreadcrumbManagerKeyedService;

// Stores breadcrumb events to and retireves them from a file on disk.
// Persisting these events allows access to breadcrumb events from previous
// application sessions.
class BreadcrumbPersistentStorageManager : public BreadcrumbManagerObserver {
 public:
  explicit BreadcrumbPersistentStorageManager(base::FilePath directory);
  ~BreadcrumbPersistentStorageManager() override;

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

  // Writes events from |observered_manager_| to |breadcrumbs_file_|,
  // overwriting any existing persisted breadcrumbs.
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

  // Individual beadcrumbs which have not yet been written to disk.
  std::string pending_breadcrumbs_;

  // The last time a breadcrumb was written to |breadcrumbs_file_|. This
  // timestamp prevents breadcrumbs from being written to disk too often.
  base::TimeTicks last_written_time_;

  // A timer to delay writing to disk too often.
  base::OneShotTimer write_timer_;

  // The path to the file for storing persisted breadcrumbs.
  base::FilePath breadcrumbs_file_path_;

  // The path to the temporary file for writing persisted breadcrumbs.
  base::FilePath breadcrumbs_temp_file_path_;

  // The current size of breadcrumbs written to |breadcrumbs_file_path_|.
  // NOTE: The optional will not have a value until the size of the existing
  // file, if any, is retrieved.
  base::Optional<size_t> current_mapped_file_position_;

  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<BreadcrumbPersistentStorageManager> weak_ptr_factory_;

  BreadcrumbPersistentStorageManager(
      const BreadcrumbPersistentStorageManager&) = delete;
  BreadcrumbPersistentStorageManager& operator=(
      const BreadcrumbPersistentStorageManager&) = delete;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
