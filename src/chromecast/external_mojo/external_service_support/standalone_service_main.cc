// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_for_io.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/external_mojo/external_service_support/process_setup.h"
#include "chromecast/external_mojo/external_service_support/service_process.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

// Simple process entrypoint for standalone Mojo services.

struct GlobalState {
  std::unique_ptr<chromecast::external_service_support::ServiceProcess>
      service_process;
  std::unique_ptr<chromecast::external_service_support::ExternalConnector>
      connector;
};

void OnConnected(
    GlobalState* state,
    std::unique_ptr<chromecast::external_service_support::ExternalConnector>
        connector) {
  state->connector = std::move(connector);
  state->service_process =
      chromecast::external_service_support::ServiceProcess::Create(
          state->connector.get());
}

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  chromecast::external_service_support::CommonProcessInitialization(argc, argv);

  base::MessageLoopForIO main_loop;
  base::RunLoop run_loop;

  mojo::core::Init();

  mojo::core::ScopedIPCSupport ipc_support(
      main_loop.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  GlobalState state;
  chromecast::external_service_support::ExternalConnector::Connect(
      chromecast::external_mojo::GetBrokerPath(),
      base::BindOnce(&OnConnected, &state));
  run_loop.Run();

  return 0;
}
