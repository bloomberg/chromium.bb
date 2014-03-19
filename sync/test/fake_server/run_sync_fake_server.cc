// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "sync/test/fake_server/fake_sync_server_http_handler.h"

// This runs a FakeSyncServerHttpHandler as a command-line app.
int main(int argc, char* argv[]) {
  using fake_server::FakeSyncServerHttpHandler;

  CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  FakeSyncServerHttpHandler server;
  base::WeakPtrFactory<FakeSyncServerHttpHandler> server_ptr_factory(&server);

  // An HttpServer must be run on a IO MessageLoop.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
  bool posted = message_loop.current()->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&FakeSyncServerHttpHandler::Start,
                 server_ptr_factory.GetWeakPtr()));
  CHECK(posted) << "Failed to start the HTTP server. PostTask returned false.";
  message_loop.current()->Run();

  return 0;
}
