// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
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

namespace forwarder2 {
namespace {

const char kLogFilePath[] = "/tmp/host_forwarder_log";
const char kPIDFilePath[] = "/tmp/host_forwarder_pid";
const char kDaemonIdentifier[] = "chrome_host_forwarder_daemon";

const char kKillServerCommand[] = "kill-server";
const char kForwardCommand[] = "forward";

const int kBufSize = 256;

// Needs to be global to be able to be accessed from the signal handler.
PipeNotifier* g_notifier = NULL;

// Lets the daemon fetch the exit notifier file descriptor.
int GetExitNotifierFD() {
  DCHECK(g_notifier);
  return g_notifier->receiver_fd();
}

void KillHandler(int signal_number) {
  char buf[kBufSize];
  if (signal_number != SIGTERM && signal_number != SIGINT) {
    snprintf(buf, sizeof(buf), "Ignoring unexpected signal %d.", signal_number);
    SIGNAL_SAFE_LOG(WARNING, buf);
    return;
  }
  snprintf(buf, sizeof(buf), "Received signal %d.", signal_number);
  SIGNAL_SAFE_LOG(WARNING, buf);
  static int s_kill_handler_count = 0;
  CHECK(g_notifier);
  // If for some reason the forwarder get stuck in any socket waiting forever,
  // we can send a SIGKILL or SIGINT three times to force it die
  // (non-nicely). This is useful when debugging.
  ++s_kill_handler_count;
  if (!g_notifier->Notify() || s_kill_handler_count > 2)
    exit(1);
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
      !base::StringToInt(command_pieces[0], adb_port) ||
      !base::StringToInt(command_pieces[1], device_port))
    return false;

  if (command_pieces.size() > 2) {
    if (!base::StringToInt(command_pieces[2], forward_to_port))
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

class ServerDelegate : public Daemon::ServerDelegate {
 public:
  ServerDelegate() : has_failed_(false) {}

  bool has_failed() const { return has_failed_; }

  // Daemon::ServerDelegate:
  virtual void Init() OVERRIDE {
    LOG(INFO) << "Starting host process daemon (pid=" << getpid() << ")";
    DCHECK(!g_notifier);
    g_notifier = new PipeNotifier();
    signal(SIGTERM, KillHandler);
    signal(SIGINT, KillHandler);
  }

  virtual void OnClientConnected(scoped_ptr<Socket> client_socket) OVERRIDE {
    char buf[kBufSize];
    const int bytes_read = client_socket->Read(buf, sizeof(buf));
    if (bytes_read <= 0) {
      if (client_socket->exited())
        return;
      PError("Read()");
      has_failed_ = true;
      return;
    }
    const std::string command(buf, bytes_read);
    int adb_port = 0;
    int device_port = 0;
    std::string forward_to_host;
    int forward_to_port = 0;
    const bool succeeded = ParseForwardCommand(
        command, &adb_port, &device_port, &forward_to_host, &forward_to_port);
    if (!succeeded) {
      has_failed_ = true;
      client_socket->WriteString(
          base::StringPrintf("ERROR: Could not parse forward command '%s'",
                             command.c_str()));
      return;
    }
    scoped_ptr<HostController> host_controller(
        new HostController(device_port, forward_to_host, forward_to_port,
                           adb_port, GetExitNotifierFD()));
    if (!host_controller->Connect()) {
      has_failed_ = true;
      client_socket->WriteString("ERROR: Connection to device failed.");
      return;
    }
    // Get the current allocated port.
    device_port = host_controller->device_port();
    LOG(INFO) << "Forwarding device port " << device_port << " to host "
              << forward_to_host << ":" << forward_to_port;
    if (!client_socket->WriteString(
        base::StringPrintf("%d:%d", device_port, forward_to_port))) {
      has_failed_ = true;
      return;
    }
    host_controller->Start();
    controllers_.push_back(host_controller.release());
  }

  virtual void OnServerExited() OVERRIDE {
    for (int i = 0; i < controllers_.size(); ++i)
      controllers_[i]->Join();
    if (controllers_.size() == 0) {
      LOG(ERROR) << "No forwarder servers could be started. Exiting.";
      has_failed_ = true;
    }
  }

 private:
  ScopedVector<HostController> controllers_;
  bool has_failed_;

  DISALLOW_COPY_AND_ASSIGN(ServerDelegate);
};

class ClientDelegate : public Daemon::ClientDelegate {
 public:
  ClientDelegate(const std::string& forward_command)
      : forward_command_(forward_command),
        has_failed_(false) {
  }

  bool has_failed() const { return has_failed_; }

  // Daemon::ClientDelegate:
  virtual void OnDaemonReady(Socket* daemon_socket) OVERRIDE {
    // Send the forward command to the daemon.
    CHECK(daemon_socket->WriteString(forward_command_));
    char buf[kBufSize];
    const int bytes_read = daemon_socket->Read(
        buf, sizeof(buf) - 1 /* leave space for null terminator */);
    CHECK_GT(bytes_read, 0);
    DCHECK(bytes_read < sizeof(buf));
    buf[bytes_read] = 0;
    base::StringPiece msg(buf, bytes_read);
    if (msg.starts_with("ERROR")) {
      LOG(ERROR) << msg;
      has_failed_ = true;
      return;
    }
    printf("%s\n", buf);
  }

 private:
  const std::string forward_command_;
  bool has_failed_;
};

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
  const char* command = NULL;
  int adb_port = 0;
  if (argc != 2) {
    PrintUsage(argv[0]);
    return 1;
  }
  if (!strcmp(argv[1], kKillServerCommand)) {
    command = kKillServerCommand;
  } else {
    command = kForwardCommand;
    if (!IsForwardCommandValid(argv[1])) {
      PrintUsage(argv[0]);
      return 1;
    }
  }

  ClientDelegate client_delegate(argv[1]);
  ServerDelegate daemon_delegate;
  Daemon daemon(
      kLogFilePath, kPIDFilePath, kDaemonIdentifier, &client_delegate,
      &daemon_delegate, &GetExitNotifierFD);

  if (command == kKillServerCommand)
    return !daemon.Kill();

  DCHECK(command == kForwardCommand);
  if (!daemon.SpawnIfNeeded())
    return 1;

  return client_delegate.has_failed() || daemon_delegate.has_failed();
}

}  // namespace
}  // namespace forwarder2

int main(int argc, char** argv) {
  return forwarder2::RunHostForwarder(argc, argv);
}
