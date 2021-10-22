// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/keystone/ksinstall.h"

#import <Foundation/Foundation.h>
#import <getopt.h>

#import <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"

namespace updater {
namespace {

class KSInstallApp : public App {
 public:
  KSInstallApp(int argc, char* argv[]) : argc_(argc), argv_(argv) {}

 private:
  ~KSInstallApp() override = default;
  void FirstTaskRun() override;

  bool ParseCommandLineOptions();
  void Uninstall(base::OnceCallback<void(int)> callback);

  int argc_;
  char** argv_;

  bool uninstall_;
};

void KSInstallApp::Uninstall(base::OnceCallback<void(int)> callback) {
  const absl::optional<base::FilePath>& keystone_path = GetKeystoneFolderPath(
      (geteuid() == 0) ? UpdaterScope::kSystem : UpdaterScope::kUser);

  if (!keystone_path) {
    PLOG(ERROR) << "Couldn't find Keystone path.";
    std::move(callback).Run(1);
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::DeletePathRecursively, *keystone_path),
      base::BindOnce(
          [](base::OnceCallback<void(int)> cb, bool result) {
            if (result) {
              std::move(cb).Run(0);
            } else {
              LOG(ERROR) << "Deleting Keystone at path failed";
              std::move(cb).Run(1);
            }
          },
          std::move(callback)));
}

bool KSInstallApp::ParseCommandLineOptions() {
  base::CommandLine::Init(argc_, argv_);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector args = command_line->GetArgs();
  if (command_line->HasSwitch("uninstall")) {
    uninstall_ = true;
    return true;
  }

  return false;
}

void KSInstallApp::FirstTaskRun() {
  if (!ParseCommandLineOptions())
    Shutdown(1);

  if ((geteuid() == 0) && (getuid() != 0)) {
    if (setuid(0) || setgid(0)) {
      LOG(ERROR) << "Can't setuid()/setgid() appropriately.";
      Shutdown(1);
    }
  }

  if (uninstall_) {
    Uninstall(base::BindOnce(&KSInstallApp::Shutdown, this));
  } else {
    LOG(ERROR) << "Expected --uninstall switch.";
    Shutdown(1);
  }
}

scoped_refptr<App> MakeKSInstallApp(int argc, char* argv[]) {
  return base::MakeRefCounted<KSInstallApp>(argc, argv);
}

}  // namespace

int KSInstallMain(int argc, char* argv[]) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  updater::InitLogging(GetUpdaterScope(), FILE_PATH_LITERAL("updater.log"));

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return MakeKSInstallApp(argc, argv)->Run();
}

}  // namespace updater
