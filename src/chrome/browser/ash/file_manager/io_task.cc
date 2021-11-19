// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task.h"

#include <vector>

#include "base/callback.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {

namespace io_task {

EntryStatus::EntryStatus(storage::FileSystemURL file_url,
                         absl::optional<base::File::Error> file_error)
    : url(file_url), error(file_error) {}
EntryStatus::~EntryStatus() = default;

EntryStatus::EntryStatus(EntryStatus&& other) = default;
EntryStatus& EntryStatus::operator=(EntryStatus&& other) = default;

ProgressStatus::ProgressStatus() = default;
ProgressStatus::~ProgressStatus() = default;

ProgressStatus::ProgressStatus(ProgressStatus&& other) = default;
ProgressStatus& ProgressStatus::operator=(ProgressStatus&& other) = default;

DummyIOTask::DummyIOTask(std::vector<storage::FileSystemURL> source_urls,
                         storage::FileSystemURL destination_folder,
                         OperationType type) {
  progress_.state = State::kQueued;
  progress_.type = type;
  progress_.destination_folder = std::move(destination_folder);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 2;

  for (auto& url : source_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }
}

DummyIOTask::~DummyIOTask() = default;

void DummyIOTask::Execute(IOTask::ProgressCallback progress_callback,
                          IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  progress_.state = State::kInProgress;
  progress_callback_.Run(progress_);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DummyIOTask::DoProgress, weak_ptr_factory_.GetWeakPtr()));
}

void DummyIOTask::DoProgress() {
  progress_.bytes_transferred = 1;
  progress_callback_.Run(progress_);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DummyIOTask::DoComplete, weak_ptr_factory_.GetWeakPtr()));
}

void DummyIOTask::DoComplete() {
  progress_.state = State::kSuccess;
  progress_.bytes_transferred = 2;
  for (auto& source : progress_.sources) {
    source.error.emplace(base::File::FILE_OK);
  }
  std::move(complete_callback_).Run(std::move(progress_));
}

void DummyIOTask::Cancel() {
  progress_.state = State::kCancelled;
}

}  // namespace io_task

}  // namespace file_manager
