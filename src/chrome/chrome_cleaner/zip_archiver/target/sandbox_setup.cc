// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/zip_archiver/target/zip_archiver_impl.h"

namespace chrome_cleaner {

namespace {

class ZipArchiverSandboxTargetHooks : public MojoSandboxTargetHooks {
 public:
  explicit ZipArchiverSandboxTargetHooks(MojoTaskRunner* mojo_task_runner);
  ~ZipArchiverSandboxTargetHooks() override;

  // Sandbox hooks

  ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) override;

 private:
  void CreateZipArchiverImpl(mojom::ZipArchiverRequest request);

  MojoTaskRunner* mojo_task_runner_;
  base::MessageLoop message_loop_;
  std::unique_ptr<ZipArchiverImpl, base::OnTaskRunnerDeleter>
      zip_archiver_impl_;
};

ZipArchiverSandboxTargetHooks::ZipArchiverSandboxTargetHooks(
    MojoTaskRunner* mojo_task_runner)
    : mojo_task_runner_(mojo_task_runner),
      zip_archiver_impl_(nullptr,
                         base::OnTaskRunnerDeleter(mojo_task_runner_)) {}

ZipArchiverSandboxTargetHooks::~ZipArchiverSandboxTargetHooks() = default;

ResultCode ZipArchiverSandboxTargetHooks::TargetDroppedPrivileges(
    const base::CommandLine& command_line) {
  mojom::ZipArchiverRequest request(ExtractSandboxMessagePipe(command_line));

  // This loop will run forever. Once the communication channel with the broker
  // process is broken, mojo error handler will abort this process.
  base::RunLoop run_loop;
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ZipArchiverSandboxTargetHooks::CreateZipArchiverImpl,
                     base::Unretained(this), std::move(request)));
  run_loop.Run();

  return RESULT_CODE_SUCCESS;
}

void ZipArchiverSandboxTargetHooks::CreateZipArchiverImpl(
    mojom::ZipArchiverRequest request) {
  // Replace the null pointer by the actual |ZipArchiverImpl|.
  // Manually use |new| here because |make_unique| doesn't work with
  // custom deleter.
  zip_archiver_impl_.reset(
      new ZipArchiverImpl(std::move(request), base::BindOnce(&EarlyExit, 1)));
}

}  // namespace

ResultCode RunZipArchiverSandboxTarget(
    const base::CommandLine& command_line,
    sandbox::TargetServices* target_services) {
  DCHECK(target_services);

  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  ZipArchiverSandboxTargetHooks target_hooks(mojo_task_runner.get());

  return RunSandboxTarget(command_line, target_services, &target_hooks);
}

}  // namespace chrome_cleaner
