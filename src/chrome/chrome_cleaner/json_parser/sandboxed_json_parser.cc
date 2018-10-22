// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/sandboxed_json_parser.h"

namespace chrome_cleaner {

SandboxedJsonParser::SandboxedJsonParser(MojoTaskRunner* mojo_task_runner,
                                         mojom::JsonParserPtr* json_parser_ptr)
    : mojo_task_runner_(mojo_task_runner), json_parser_ptr_(json_parser_ptr) {}

void SandboxedJsonParser::Parse(const std::string& json,
                                ParseDoneCallback callback) {
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](mojom::JsonParserPtr* json_parser_ptr,
                        const std::string& json, ParseDoneCallback callback) {
                       (*json_parser_ptr)->Parse(json, std::move(callback));
                     },
                     json_parser_ptr_, json, std::move(callback)));
}

}  // namespace chrome_cleaner
