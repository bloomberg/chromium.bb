// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/service_worker_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

ServiceWorkerTask::ServiceWorkerTask(
    const content::ServiceWorkerRunningInfo& service_worker_running_info,
    base::ProcessHandle handle)
    : Task(
          l10n_util::GetStringFUTF16(
              IDS_TASK_MANAGER_SERVICE_WORKER_PREFIX,
              base::UTF8ToUTF16(service_worker_running_info.script_url.spec())),
          service_worker_running_info.script_url.spec(),
          nullptr /* icon */,
          handle),
      render_process_id_(service_worker_running_info.process_id) {}

ServiceWorkerTask::~ServiceWorkerTask() {}

Task::Type ServiceWorkerTask::GetType() const {
  return Task::SERVICE_WORKER;
}

int ServiceWorkerTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

}  // namespace task_manager
