// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"

#include <utility>

#include "base/bind.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"

namespace chrome_cleaner {

ParserSandboxSetupHooks::ParserSandboxSetupHooks(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    base::OnceClosure connection_error_handler)
    : mojo_task_runner_(mojo_task_runner),
      connection_error_handler_(std::move(connection_error_handler)),
      parser_ptr_(new mojom::ParserPtr(),
                  base::OnTaskRunnerDeleter(mojo_task_runner_)) {}

ParserSandboxSetupHooks::~ParserSandboxSetupHooks() = default;

ResultCode ParserSandboxSetupHooks::UpdateSandboxPolicy(
    sandbox::TargetPolicy* policy,
    base::CommandLine* command_line) {
  // Unretained reference is safe because the parser_ptr is taken by the
  // caller and is expected to retain it for the life of the sandboxed process.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ParserSandboxSetupHooks::BindParserPtr,
                                base::Unretained(this),
                                SetupSandboxMessagePipe(policy, command_line),
                                base::Unretained(parser_ptr_.get())));

  return RESULT_CODE_SUCCESS;
}

void ParserSandboxSetupHooks::BindParserPtr(
    mojo::ScopedMessagePipeHandle pipe_handle,
    mojom::ParserPtr* parser_ptr) {
  parser_ptr->Bind(mojom::ParserPtrInfo(std::move(pipe_handle), 0));
  parser_ptr->set_connection_error_handler(
      std::move(connection_error_handler_));
}

UniqueParserPtr ParserSandboxSetupHooks::TakeParserPtr() {
  return std::move(parser_ptr_);
}

ResultCode SpawnParserSandbox(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    UniqueParserPtr* parser_ptr) {
  auto error_handler =
      base::BindOnce(connection_error_callback, SandboxType::kParser);
  ParserSandboxSetupHooks setup_hooks(mojo_task_runner,
                                      std::move(error_handler));
  ResultCode result_code = SpawnSandbox(&setup_hooks, SandboxType::kParser);
  *parser_ptr = setup_hooks.TakeParserPtr();

  return result_code;
}

}  // namespace chrome_cleaner
