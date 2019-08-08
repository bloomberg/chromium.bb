// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace updater {

namespace {

// For now, use the Flash CRX for testing.
// CRX id is mimojjlkmoijpicakmndhoigimigcmbb.
const uint8_t mimo_hash[] = {0xc8, 0xce, 0x99, 0xba, 0xce, 0x89, 0xf8, 0x20,
                             0xac, 0xd3, 0x7e, 0x86, 0x8c, 0x86, 0x2c, 0x11,
                             0xb9, 0x40, 0xc5, 0x55, 0xaf, 0x08, 0x63, 0x70,
                             0x54, 0xf9, 0x56, 0xd3, 0xe7, 0x88, 0xba, 0x8c};

void ThreadPoolStart() {
  base::ThreadPool::Create("Updater");
  const auto thread_pool_init_params =
      std::make_unique<base::ThreadPool::InitParams>(
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
              base::TimeDelta::FromSeconds(30)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
              base::TimeDelta::FromSeconds(30)));
  base::ThreadPool::GetInstance()->Start(*thread_pool_init_params);
}

void ThreadPoolStop() {
  base::ThreadPool::GetInstance()->Shutdown();
}

void QuitLoop(base::OnceClosure quit_closure) {
  std::move(quit_closure).Run();
}

class Observer : public update_client::UpdateClient::Observer {
 public:
  explicit Observer(scoped_refptr<update_client::UpdateClient> update_client)
      : update_client_(update_client) {}

  // Overrides for update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override {
    update_client::CrxUpdateItem item;
    update_client_->GetCrxUpdateState(id, &item);
  }

 private:
  scoped_refptr<update_client::UpdateClient> update_client_;
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// The log file is created in DIR_LOCAL_APP_DATA or DIR_APP_DATA.
void InitLogging(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  base::FilePath log_dir;
  GetProductDataDirectory(&log_dir);
  const auto log_file = log_dir.Append(FILE_PATH_LITERAL("updater.log"));
  settings.log_file = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // enable_process_id
                       true,    // enable_thread_id
                       true,    // enable_timestamp
                       false);  // enable_tickcount
  VLOG(1) << "Log file " << settings.log_file;
}

void InitializeUpdaterMain() {
  crash_reporter::InitializeCrashKeys();

  static crash_reporter::CrashKeyString<16> crash_key_process_type(
      "process_type");
  crash_key_process_type.Set("updater");

  if (CrashClient::GetInstance()->InitializeCrashReporting()) {
    VLOG(1) << "Crash reporting initialized.";
  } else {
    VLOG(1) << "Crash reporting is not available.";
  }
  StartCrashReporter(UPDATER_VERSION_STRING);

  ThreadPoolStart();
}

void TerminateUpdaterMain() {
  ThreadPoolStop();
}

}  // namespace

int UpdaterMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestSwitch))
    return 0;

  InitLogging(*command_line);

  if (command_line->HasSwitch(kCrashHandlerSwitch)) {
    return CrashReporterMain();
  }

  InitializeUpdaterMain();

  if (command_line->HasSwitch(kCrashMeSwitch)) {
    int* ptr = nullptr;
    return *ptr;
  }

  base::MessageLoopForUI message_loop;
  base::RunLoop runloop;
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  auto config = base::MakeRefCounted<Configurator>();

  {
    base::ScopedDisallowBlocking no_blocking_allowed;

    auto update_client = update_client::UpdateClientFactory(config);

    Observer observer(update_client);
    update_client->AddObserver(&observer);

    const std::vector<std::string> ids = {"mimojjlkmoijpicakmndhoigimigcmbb"};
    update_client->Update(
        ids,
        base::BindOnce(
            [](const std::vector<std::string>& ids)
                -> std::vector<base::Optional<update_client::CrxComponent>> {
              update_client::CrxComponent component;
              component.name = "mimo";
              component.pk_hash.assign(std::begin(mimo_hash),
                                       std::end(mimo_hash));
              component.version = base::Version("0.0");
              component.requires_network_encryption = false;
              return {component};
            }),
        true,
        base::BindOnce(
            [](base::OnceClosure closure, update_client::Error error) {
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, base::BindOnce(&QuitLoop, std::move(closure)));
            },
            runloop.QuitWhenIdleClosure()));

    runloop.Run();

    update_client->RemoveObserver(&observer);
    update_client = nullptr;
  }

  {
    base::RunLoop runloop;
    config->GetPrefService()->CommitPendingWrite(base::BindOnce(
        [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
        runloop.QuitWhenIdleClosure()));
    runloop.Run();
  }

  TerminateUpdaterMain();
  return 0;
}

}  // namespace updater
