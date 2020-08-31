// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/action_handler.h"
#include "components/update_client/update_client.h"

namespace updater {

class ActionHandler : public update_client::ActionHandler {
 public:
  ActionHandler() = default;

 private:
  using Result =
      std::tuple<bool /*succeeded*/, int /*error_code*/, int /*extra_code*/>;
  ~ActionHandler() override = default;

  // Overrides for update_client::ActionHandler.
  void Handle(const base::FilePath& action,
              const std::string& session_id,
              Callback callback) override;

  // For Windows, the action is a path to an EXE file.
  static Result RunCommand(const base::FilePath& exe_path);

  ActionHandler(const ActionHandler&) = delete;
  ActionHandler& operator=(const ActionHandler&) = delete;
};

void ActionHandler::Handle(const base::FilePath& action,
                           const std::string&,
                           Callback callback) {
  DCHECK(!action.empty());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ActionHandler::RunCommand, action),
      base::BindOnce(
          [](Callback callback, const Result& result) {
            bool succeeded = false;
            int error_code = 0;
            int extra_code = 0;
            std::tie(succeeded, error_code, extra_code) = result;
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), succeeded,
                                          error_code, extra_code));
          },
          std::move(callback)));
}

ActionHandler::Result ActionHandler::RunCommand(
    const base::FilePath& exe_path) {
  base::CommandLine command_line(exe_path);
  VLOG(1) << "run command: " << command_line.GetCommandLineString();
  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchProcess(command_line, options);
  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  return Result{succeeded, exit_code, 0};
}

scoped_refptr<update_client::ActionHandler> MakeActionHandler() {
  return base::MakeRefCounted<ActionHandler>();
}

}  // namespace updater
