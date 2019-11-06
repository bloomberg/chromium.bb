// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/test/ftl_services_playground.h"

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  base::MessageLoopForIO message_loop;
  remoting::FtlServicesPlayground playground;

  if (playground.ShouldPrintHelp()) {
    playground.PrintHelp();
    return 0;
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "FtlServicesPlayground");
  mojo::core::Init();

  playground.StartAndAuthenticate();

  return 0;
}
