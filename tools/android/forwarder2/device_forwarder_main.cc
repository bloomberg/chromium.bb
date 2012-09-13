// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "base/logging.h"
#include "base/command_line.h"
#include "base/stringprintf.h"
#include "tools/android/common/daemon.h"
#include "tools/android/forwarder2/device_controller.h"
#include "tools/android/forwarder2/pipe_notifier.h"

namespace {

// Leaky global instance, accessed from the signal handler.
forwarder2::PipeNotifier* g_notifier = NULL;

// Unix domain socket name for Device Controller.
const char kDefaultAdbSocket[] = "chrome_device_forwarder";

void KillHandler(int /* unused */) {
  CHECK(g_notifier);
  if (!g_notifier->Notify())
    exit(-1);
}

}  // namespace

int main(int argc, char** argv) {
  printf("Device forwarder to forward connections to the Host machine.\n");
  printf("Like 'adb forward' but in the reverse direction\n");

  CommandLine command_line(argc, argv);
  std::string adb_socket_path = command_line.GetSwitchValueASCII("adb_sock");
  if (adb_socket_path.empty())
    adb_socket_path = kDefaultAdbSocket;
  if (tools::HasHelpSwitch(command_line)) {
    tools::ShowHelp(
        argv[0],
        "[--adb_sock=<adb sock>]",
        base::StringPrintf(
            "  <adb sock> is the Abstract Unix Domain Socket path "
            " where Adb is configured to forward from."
            " Default is %s\n", kDefaultAdbSocket).c_str());
    return 0;
  }
  if (!tools::HasNoSpawnDaemonSwitch(command_line))
    tools::SpawnDaemon(0);

  g_notifier = new forwarder2::PipeNotifier();

  signal(SIGTERM, KillHandler);
  signal(SIGINT, KillHandler);
  CHECK(g_notifier);
  forwarder2::DeviceController controller(g_notifier->receiver_fd());
  printf("Starting Device Forwarder.\n");
  controller.Start(adb_socket_path);

  return 0;
}
