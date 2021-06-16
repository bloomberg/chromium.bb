// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_MAIN_RUNNER_HANDLER_H_
#define CEF_LIBCEF_COMMON_MAIN_RUNNER_HANDLER_H_
#pragma once

namespace content {
struct MainFunctionParams;
}

// Handles running of the main process.
class CefMainRunnerHandler {
 public:
  virtual void PreCreateMainMessageLoop() = 0;
  virtual int RunMainProcess(
      const content::MainFunctionParams& main_function_params) = 0;

 protected:
  virtual ~CefMainRunnerHandler() {}
};

#endif  // CEF_LIBCEF_COMMON_MAIN_RUNNER_HANDLER_H_
