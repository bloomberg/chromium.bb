// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "tools/android/common/daemon.h"
#include "tools/android/forwarder2/host_controller.h"
#include "tools/android/forwarder2/pipe_notifier.h"

using base::StringToInt;
using forwarder2::HostController;

namespace {

const int kDefaultAdbPort = 3000;

// Need to be global to be able to accessed from the signal handler.
forwarder2::PipeNotifier* g_notifier;

void KillHandler(int /* unused */) {
  CHECK(g_notifier);
  g_notifier->Notify();
}

// Format of arg: <Device port>[:<Forward to port>:<Forward to address>]
bool ParseForwardArg(const std::string& arg,
                     int* device_port,
                     std::string* forward_to_host,
                     int* forward_to_port) {
  std::vector<std::string> arg_pieces;
  base::SplitString(arg, ':', &arg_pieces);
  if (arg_pieces.size() == 0 || !StringToInt(arg_pieces[0], device_port))
    return false;

  if (*device_port == 0) {
    // TODO(felipeg): handle the case where we want to dynamically allocate the
    // port. Althogh the Socket already supports it, we have to
    // communicate the port from one forwarder to the other.
    LOG(ERROR) << "Dynamically allocate the port is not yet implemented.";
    return false;
  }

  if (arg_pieces.size() > 1) {
    if (!StringToInt(arg_pieces[1], forward_to_port))
      return false;
    if (arg_pieces.size() > 2)
      *forward_to_host = arg_pieces[2];
  } else {
    *forward_to_port = *device_port;
  }

  printf("Forwarding device port %d to host %d:%s\n",
         *device_port, *forward_to_port, forward_to_host->c_str());
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  printf("Host forwarder to handle incoming connections from Android.\n");
  printf("Like 'adb forward' but in the reverse direction\n");

  CommandLine command_line(argc, argv);
  bool show_help = tools::HasHelpSwitch(command_line);
  std::string adb_port_str = command_line.GetSwitchValueASCII("adb_port");
  int adb_port = kDefaultAdbPort;
  if (!adb_port_str.empty() && !StringToInt(adb_port_str, &adb_port)) {
    printf("Could not parse adb port number: %s\n", adb_port_str.c_str());
    show_help = true;
  }
  CommandLine::StringVector forward_args = command_line.GetArgs();
  if (show_help || forward_args.empty()) {
    tools::ShowHelp(
        argv[0],
        "[--adb_port=<adb port>] "
        "<Device port>[:<Forward to port>:<Forward to address>] ...",
        base::StringPrintf(
            "  <adb port> is the TCP port Adb is configured to forward to."
            " Default is %d\n"
            "  <Forward to port> default is <Device port>\n"
            "  <Forward to address> default is 127.0.0.1.",
            kDefaultAdbPort).c_str());
    return 0;
  }

  g_notifier = new forwarder2::PipeNotifier();
  ScopedVector<HostController> controllers;
  int failed_count = 0;
  for (size_t i = 0; i < forward_args.size(); ++i) {
    int device_port = 0;
    std::string forward_to_host;
    int forward_to_port = 0;
    if (ParseForwardArg(forward_args[i],
                        &device_port,
                        &forward_to_host,
                        &forward_to_port)) {
      HostController* host_controller(
          new HostController(device_port,
                             forward_to_host,
                             forward_to_port,
                             adb_port,
                             g_notifier->receiver_fd()));
      host_controller->Start();
      controllers.push_back(host_controller);
    } else {
      printf("Couldn't start forwarder server for port spec: %s\n",
             forward_args[i].c_str());
      ++failed_count;
    }
  }

  // Signal handler must be installed after the for loop above where we start
  // the host_controllers and push_back into the vector. Otherwise a race
  // condition may occur.
  signal(SIGTERM, KillHandler);
  signal(SIGINT, KillHandler);

  if (controllers.size() == 0) {
    printf("No forwarder servers could be started. Exiting.\n");
    return failed_count;
  }

  // TODO(felipeg): We should check if the controllers are really alive before
  // printing Ready.
  printf("Host Forwarder Ready.\n");
  for (int i = 0; i < controllers.size(); ++i)
    controllers[i]->Join();

  return 0;
}
