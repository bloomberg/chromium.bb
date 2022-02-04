// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager {

namespace io_task {

class IOTaskController;

enum class State {
  // Task has been queued, but not yet started.
  kQueued,

  // Task is currently running.
  kInProgress,

  // Task has been successfully completed.
  kSuccess,

  // Task has completed with errors.
  kError,

  // Task has been canceled without finishing.
  kCancelled,
};

enum class OperationType {
  kCopy,
  kDelete,
  kExtract,
  kMove,
  kZip,
};

// Unique identifier for any type of task.
using IOTaskId = uint64_t;

// Represents the status of a particular entry in an I/O task.
struct EntryStatus {
  EntryStatus(storage::FileSystemURL file_url,
              absl::optional<base::File::Error> file_error);
  ~EntryStatus();

  EntryStatus(EntryStatus&& other);
  EntryStatus& operator=(EntryStatus&& other);

  storage::FileSystemURL url;
  // May be empty if the entry has not been fully processed yet.
  absl::optional<base::File::Error> error;
};

// Represents the current progress of an I/O task.
struct ProgressStatus {
  // Out-of-line constructors to appease the style linter.
  ProgressStatus();
  ProgressStatus(const ProgressStatus& other) = delete;
  ProgressStatus& operator=(const ProgressStatus& other) = delete;
  ~ProgressStatus();

  // Allow ProgressStatus to be moved.
  ProgressStatus(ProgressStatus&& other);
  ProgressStatus& operator=(ProgressStatus&& other);

  // Task state.
  State state;

  // I/O Operation type (e.g. copy, move).
  OperationType type;

  // Files the operation processes.
  std::vector<EntryStatus> sources;

  // Entries created by the I/O task. These files aren't necessarily related to
  // |sources|.
  std::vector<EntryStatus> outputs;

  // Optional destination folder for operations that transfer files to a
  // directory (e.g. copy or move).
  storage::FileSystemURL destination_folder;

  // ProgressStatus over all |sources|.
  int64_t bytes_transferred = 0;

  // Total size of all |sources|.
  int64_t total_bytes = 0;

  // The task id for this progress status.
  IOTaskId task_id = 0;

  // The estimate time to finish the operation.
  double remaining_seconds = 0;
};

// An IOTask represents an I/O operation over multiple files, and is responsible
// for executing the operation and providing progress/completion reports.
class IOTask {
 public:
  virtual ~IOTask() = default;

  using ProgressCallback = base::RepeatingCallback<void(const ProgressStatus&)>;
  using CompleteCallback = base::OnceCallback<void(ProgressStatus)>;

  // Executes the task. |progress_callback| should be called every so often to
  // give updates, and |complete_callback| should be only called once at the end
  // to signify completion with a |kSuccess|, |kError| or |kCancelled| state.
  // |progress_callback| should be called on the same sequeuence Execute() was.
  virtual void Execute(ProgressCallback progress_callback,
                       CompleteCallback complete_callback) = 0;

  // Cancels the task. This should set the progress state to be |kCancelled|,
  // but not call any of Execute()'s callbacks. The task will be deleted
  // synchronously after this call returns.
  virtual void Cancel() = 0;

  // Gets the current progress status of the task.
  const ProgressStatus& progress() { return progress_; }

 protected:
  IOTask() = default;
  IOTask(const IOTask& other) = delete;
  IOTask& operator=(const IOTask& other) = delete;

  ProgressStatus progress_;

  // Task Controller can update `progress_`.
  friend class IOTaskController;
};

// No-op IO Task for testing.
// TODO(austinct): Move into io_task_controller_unittest file when the other
// IOTasks have been implemented.
class DummyIOTask : public IOTask {
 public:
  DummyIOTask(std::vector<storage::FileSystemURL> source_urls,
              storage::FileSystemURL destination_folder,
              OperationType type);
  ~DummyIOTask() override;

  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

 private:
  void DoProgress();
  void DoComplete();

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<DummyIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
