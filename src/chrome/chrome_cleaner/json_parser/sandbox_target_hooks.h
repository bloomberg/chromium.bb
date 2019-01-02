// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_JSON_PARSER_SANDBOX_TARGET_HOOKS_H_
#define CHROME_CHROME_CLEANER_JSON_PARSER_SANDBOX_TARGET_HOOKS_H_

#include <memory>

#include "base/command_line.h"
#include "chrome/chrome_cleaner/interfaces/json_parser.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/json_parser/json_parser_impl.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Hooks to initialize the sandboxed JSON parser process by creating a
// JsonParserImpl instance and binding it to the mojo pipe.
class JsonParserSandboxTargetHooks : public MojoSandboxTargetHooks {
 public:
  explicit JsonParserSandboxTargetHooks(MojoTaskRunner* mojo_task_runner);
  ~JsonParserSandboxTargetHooks() override;

  // SandboxTargetHooks
  ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) override;

 private:
  void CreateJsonParserImpl(mojom::JsonParserRequest request);

  MojoTaskRunner* mojo_task_runner_;
  base::MessageLoop message_loop_;
  std::unique_ptr<JsonParserImpl> json_parser_impl_;

  DISALLOW_COPY_AND_ASSIGN(JsonParserSandboxTargetHooks);
};

// Run the JSON parser, to be called in a newly spawned process.
ResultCode RunJsonParserSandbox(const base::CommandLine& command_line,
                                sandbox::TargetServices* target_services);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_JSON_PARSER_SANDBOX_TARGET_HOOKS_H_
