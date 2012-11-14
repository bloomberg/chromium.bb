// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <vector>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/posix/eintr_wrapper.h"
#include "base/safe_strerror_posix.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "tools/android/forwarder2/common.h"
#include "tools/android/forwarder2/daemon.h"
#include "tools/android/forwarder2/host_controller.h"
#include "tools/android/forwarder2/pipe_notifier.h"
#include "tools/android/forwarder2/socket.h"

using base::StringToInt;

namespace forwarder2 {
namespace {

const char kPIDFilePath[] = "/tmp/host_forwarder_pid";
const char kCommandSocketPath[] = "host_forwarder_command_socket";
const char kWelcomeMessage[] = "forwarder2";
const int kBufSize = 256;

// Need to be global to be able to accessed from the signal handler.
PipeNotifier* g_notifier;

void KillHandler(int signal_number) {
  if (signal_number != SIGTERM && signal_number != SIGINT) {
    char buf[kBufSize];
    snprintf(buf, sizeof(buf), "Ignoring unexpected signal %d.", signal_number);
    SIGNAL_SAFE_LOG(WARNING, buf);
    return;
  }
  static int s_kill_handler_count = 0;
  CHECK(g_notifier);
  // If for some reason the forwarder get stuck in any socket waiting forever,
  // we can send a SIGKILL or SIGINT three times to force it die
  // (non-nicely). This is useful when debugging.
  ++s_kill_handler_count;
  if (!g_notifier->Notify() || s_kill_handler_count > 2)
    exit(1);
}

enum {
  kConnectSingleTry = 1,
  kConnectNoIdleTime = 0,
};

scoped_ptr<Socket> ConnectToDaemon(int tries_count, int idle_time_msec) {
  for (int i = 0; i < tries_count; ++i) {
    scoped_ptr<Socket> socket(new Socket());
    if (!socket->ConnectUnix(kCommandSocketPath, true)) {
      if (idle_time_msec)
        usleep(idle_time_msec * 1000);
      continue;
    }
    char buf[sizeof(kWelcomeMessage)];
    memset(buf, 0, sizeof(buf));
    if (socket->Read(buf, sizeof(buf)) < 0) {
      perror("read");
      continue;
    }
    if (strcmp(buf, kWelcomeMessage)) {
      LOG(ERROR) << "Unexpected message read from daemon: " << buf;
      break;
    }
    return socket.Pass();
  }
  return scoped_ptr<Socket>(NULL);
}

// Format of |command|:
//   <ADB port>:<Device port>[:<Forward to port>:<Forward to address>].
bool ParseForwardCommand(const std::string& command,
                         int* adb_port,
                         int* device_port,
                         std::string* forward_to_host,
                         int* forward_to_port) {
  std::vector<std::string> command_pieces;
  base::SplitString(command, ':', &command_pieces);

  if (command_pieces.size() < 2 ||
      !StringToInt(command_pieces[0], adb_port) ||
      !StringToInt(command_pieces[1], device_port))
    return false;

  if (command_pieces.size() > 2) {
    if (!StringToInt(command_pieces[2], forward_to_port))
      return false;
    if (command_pieces.size() > 3)
      *forward_to_host = command_pieces[3];
  } else {
    *forward_to_port = *device_port;
  }
  return true;
}

bool IsForwardCommandValid(const std::string& command) {
  int adb_port, device_port, forward_to_port;
  std::string forward_to_host;
  std::vector<std::string> command_pieces;
  return ParseForwardCommand(
      command, &adb_port, &device_port, &forward_to_host, &forward_to_port);
}

bool DaemonHandler() {
  LOG(INFO) << "Starting host process daemon (pid=" << getpid() << ")";
  DCHECK(!g_notifier);
  g_notifier = new PipeNotifier();

  const int notifier_fd = g_notifier->receiver_fd();
  Socket command_socket;
  if (!command_socket.BindUnix(kCommandSocketPath, true)) {
    LOG(ERROR) << "Could not bind Unix Domain Socket";
    return false;
  }
  command_socket.set_exit_notifier_fd(notifier_fd);

  signal(SIGTERM, KillHandler);
  signal(SIGINT, KillHandler);

  ScopedVector<HostController> controllers;
  int failed_count = 0;

  for (;;) {
    Socket client_socket;
    if (!command_socket.Accept(&client_socket)) {
      if (command_socket.exited())
        return true;
      PError("Accept()");
      return false;
    }
    if (!client_socket.Write(kWelcomeMessage, sizeof(kWelcomeMessage))) {
      PError("Write()");
      continue;
    }
    char buf[kBufSize];
    const int bytes_read = client_socket.Read(buf, sizeof(buf));
    if (bytes_read <= 0) {
      if (client_socket.exited())
        break;
      PError("Read()");
      ++failed_count;
    }
    const std::string command(buf, bytes_read);
    int adb_port = 0;
    int device_port = 0;
    std::string forward_to_host;
    int forward_to_port = 0;
    const bool succeeded = ParseForwardCommand(
        command, &adb_port, &device_port, &forward_to_host, &forward_to_port);
    if (!succeeded) {
      ++failed_count;
      client_socket.WriteString(
          base::StringPrintf("ERROR: Could not parse forward command '%s'",
                             command.c_str()));
      continue;
    }
    scoped_ptr<HostController> host_controller(
        new HostController(device_port, forward_to_host, forward_to_port,
                           adb_port, notifier_fd));
    if (!host_controller->Connect()) {
      ++failed_count;
      client_socket.WriteString("ERROR: Connection to device failed.");
      continue;
    }
    // Get the current allocated port.
    device_port = host_controller->device_port();
    LOG(INFO) << "Forwarding device port " << device_port << " to host "
              << forward_to_host << ":" << forward_to_port;
    if (!client_socket.WriteString(
        base::StringPrintf("%d:%d", device_port, forward_to_port))) {
      ++failed_count;
      continue;
    }
    host_controller->Start();
    controllers.push_back(host_controller.release());
  }
  for (int i = 0; i < controllers.size(); ++i)
    controllers[i]->Join();

  if (controllers.size() == 0) {
    LOG(ERROR) << "No forwarder servers could be started. Exiting.";
    return false;
  }
  return true;
}

void PrintUsage(const char* program_name) {
  LOG(ERROR) << program_name << " adb_port:from_port:to_port:to_host\n"
      "<adb port> is the TCP port Adb is configured to forward to.";
}

int RunHostForwarder(int argc, char** argv) {
  if (!CommandLine::Init(argc, argv)) {
    LOG(ERROR) << "Could not initialize command line";
    return 1;
  }
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string command;
  int adb_port = 0;
  if (argc != 2) {
    PrintUsage(argv[0]);
    return 1;
  }
  if (!strcmp(argv[1], "kill-server")) {
    command = "kill-server";
  } else {
    command = "forward";
    if (!IsForwardCommandValid(argv[1])) {
      PrintUsage(argv[0]);
      return 1;
    }
  }

  Daemon daemon(kPIDFilePath);

  if (command == "kill-server")
    return !daemon.Kill();

  bool is_daemon = false;
  scoped_ptr<Socket> daemon_socket = ConnectToDaemon(
      kConnectSingleTry, kConnectNoIdleTime);
  if (!daemon_socket) {
    if (!daemon.Spawn(&is_daemon))
      return 1;
  }

  if (is_daemon)
    return !DaemonHandler();

  if (!daemon_socket) {
    const int kTries = 10;
    const int kIdleTimeMsec = 10;
    daemon_socket = ConnectToDaemon(kTries, kIdleTimeMsec);
    if (!daemon_socket) {
      LOG(ERROR) << "Could not connect to daemon.";
      return 1;
    }
  }

  // Send the forward command to the daemon.
  CHECK(daemon_socket->Write(argv[1], strlen(argv[1])));
  char buf[kBufSize];
  const int bytes_read = daemon_socket->Read(
      buf, sizeof(buf) - 1 /* leave space for null terminator */);
  CHECK_GT(bytes_read, 0);
  DCHECK(bytes_read < sizeof(buf));
  buf[bytes_read] = 0;
  base::StringPiece msg(buf, bytes_read);
  if (msg.starts_with("ERROR")) {
    LOG(ERROR) << msg;
    return 1;
  }
  printf("%s\n", buf);
  return 0;
}

}  // namespace
}  // namespace forwarder2

int main(int argc, char** argv) {
  return forwarder2::RunHostForwarder(argc, argv);
}
