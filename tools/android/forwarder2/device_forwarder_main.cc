// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "tools/android/forwarder2/common.h"
#include "tools/android/forwarder2/daemon.h"
#include "tools/android/forwarder2/device_controller.h"
#include "tools/android/forwarder2/pipe_notifier.h"

namespace forwarder2 {
namespace {

// Leaky global instance, accessed from the signal handler.
forwarder2::PipeNotifier* g_notifier = NULL;

const int kBufSize = 256;

const char kPIDFilePath[] = "/data/local/tmp/chrome_device_forwarder_pid";
const char kDaemonIdentifier[] = "chrome_device_forwarder_daemon";

const char kKillServerCommand[] = "kill-server";
const char kStartCommand[] = "start";

void KillHandler(int /* unused */) {
  CHECK(g_notifier);
  if (!g_notifier->Notify())
    exit(1);
}

// Lets the daemon fetch the exit notifier file descriptor.
int GetExitNotifierFD() {
  DCHECK(g_notifier);
  return g_notifier->receiver_fd();
}

class ServerDelegate : public Daemon::ServerDelegate {
 public:
  // Daemon::ServerDelegate:
  virtual void Init() OVERRIDE {
    DCHECK(!g_notifier);
    g_notifier = new forwarder2::PipeNotifier();
    signal(SIGTERM, KillHandler);
    signal(SIGINT, KillHandler);
    controller_thread_.reset(new base::Thread("controller_thread"));
    controller_thread_->Start();
  }

  virtual void OnClientConnected(scoped_ptr<Socket> client_socket) OVERRIDE {
    char buf[kBufSize];
    const int bytes_read = client_socket->Read(buf, sizeof(buf));
    if (bytes_read <= 0) {
      if (client_socket->exited())
        return;
      PError("Read()");
      return;
    }
    const std::string adb_socket_path(buf, bytes_read);
    if (adb_socket_path == adb_socket_path_) {
      client_socket->WriteString("OK");
      return;
    }
    if (!adb_socket_path_.empty()) {
      client_socket->WriteString(
          base::StringPrintf(
              "ERROR: Device controller already running (adb_socket_path=%s)",
              adb_socket_path_.c_str()));
      return;
    }
    adb_socket_path_ = adb_socket_path;
    controller_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ServerDelegate::StartController, adb_socket_path,
                   GetExitNotifierFD(), base::Passed(&client_socket)));
  }

  virtual void OnServerExited() OVERRIDE {}

 private:
  static void StartController(const std::string& adb_socket_path,
                              int exit_notifier_fd,
                              scoped_ptr<Socket> client_socket) {
    forwarder2::DeviceController controller(exit_notifier_fd);
    if (!controller.Init(adb_socket_path)) {
      client_socket->WriteString(
          base::StringPrintf("ERROR: Could not initialize device controller "
                             "with ADB socket path: %s",
                             adb_socket_path.c_str()));
      return;
    }
    client_socket->WriteString("OK");
    client_socket->Close();
    // Note that the following call is blocking which explains why the device
    // controller has to live on a separate thread (so that the daemon command
    // server is not blocked).
    controller.Start();
  }

  base::AtExitManager at_exit_manager_;  // Used by base::Thread.
  scoped_ptr<base::Thread> controller_thread_;
  std::string adb_socket_path_;
};

class ClientDelegate : public Daemon::ClientDelegate {
 public:
  ClientDelegate(const std::string& adb_socket)
      : adb_socket_(adb_socket),
        has_failed_(false) {
  }

  bool has_failed() const { return has_failed_; }

  // Daemon::ClientDelegate:
  virtual void OnDaemonReady(Socket* daemon_socket) OVERRIDE {
    // Send the adb socket path to the daemon.
    CHECK(daemon_socket->Write(adb_socket_.c_str(),
                               adb_socket_.length()));
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
  }

 private:
  const std::string adb_socket_;
  bool has_failed_;
};

int RunDeviceForwarder(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr,
            "Usage: %s kill-server|<adb_socket>\n"
            "  <adb_socket> is the abstract Unix Domain Socket path "
            "where Adb is configured to forward from.\n", argv[0]);
    return 1;
  }
  CommandLine::Init(argc, argv);  // Needed by logging.
  const char* const command =
      !strcmp(argv[1], kKillServerCommand) ? kKillServerCommand : kStartCommand;
  ClientDelegate client_delegate(argv[1]);
  ServerDelegate daemon_delegate;
  const char kLogFilePath[] = "";  // Log to logcat.
  Daemon daemon(kLogFilePath, kPIDFilePath, kDaemonIdentifier, &client_delegate,
                &daemon_delegate, &GetExitNotifierFD);

  if (command == kKillServerCommand)
    return !daemon.Kill();

  DCHECK(command == kStartCommand);
  if (!daemon.SpawnIfNeeded())
    return 1;
  return client_delegate.has_failed();
}

}  // namespace
}  // namespace forwarder2

int main(int argc, char** argv) {
  return forwarder2::RunDeviceForwarder(argc, argv);
}
