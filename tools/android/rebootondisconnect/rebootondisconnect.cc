// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "tools/android/common/adb_connection.h"
#include "tools/android/common/daemon.h"
#include "tools/android/common/net.h"

namespace {

const pthread_t kInvalidThread = static_cast<pthread_t>(-1);
bool g_killed = false;
const int kMessagePeriodSeconds = 2;
const int kMessageTimeoutSeconds = 1;
const int kWarmupTimeInSeconds = 60;
const int kMaxFailedPingRequests = 5;

class PingThread {
 public:
  PingThread()
      : thread_(kInvalidThread),
        requests_issued_(0),
        subsequent_requests_failed_(0) {
  }

  ~PingThread() {
  }

  void DumpInformation() {
    LOG(INFO) << "RebootOnDisconnect information: ";
    LOG(INFO) << "Ping attempts made: " << requests_issued_;
    LOG(INFO) << "Subsequent attempts failed: " << subsequent_requests_failed_;
  }

  void Start() {
    pthread_create(&thread_, NULL, WorkerThread, this);
  }

  void Join() {
    if (thread_ != kInvalidThread)
      pthread_join(thread_, NULL);
  }

 private:
  void PingSucceeded() {
    subsequent_requests_failed_ = 0;
  }

  void PingFailed() {
    subsequent_requests_failed_++;
  }

  void OnRequestIssued() {
    requests_issued_++;
  }

  bool ShouldReboot() {
    // We should reboot if more than kMaxFailedPingRequests requests have
    // failed, but only if we've made at least warmupPingRequests already.
    const int warmupPingRequests = kWarmupTimeInSeconds / kMessagePeriodSeconds;
    bool reboot = false;
    if (requests_issued_ >= warmupPingRequests)
      reboot = (subsequent_requests_failed_ >= kMaxFailedPingRequests);
    return reboot;
  }

  bool DoPing(int socket);

  static void* WorkerThread(void* arg);
  static void DoReboot();

  pthread_t thread_;

  volatile int requests_issued_;
  volatile int subsequent_requests_failed_;
};

bool PingThread::DoPing(int socket) {
  bool succeeded = false;
  pollfd fds;
  time_t start_time = time(NULL);
  const size_t buf_size = sizeof(start_time) / sizeof(char);
  char buf[buf_size];
  char buf_recv[buf_size + 1];

  memset(buf_recv, 0, buf_size + 1);

  // Use the current time to form a unique data packet.
  memcpy(buf, &start_time, sizeof(start_time));

  memset(&fds, 0, sizeof(fds));
  fds.fd = socket;
  fds.events = POLLIN | POLLERR | POLLHUP;
  fds.revents = 0;

  do {
    // Send the message that we want echoed back to us.
    if (HANDLE_EINTR(send(socket, buf, buf_size, 0)) < 0) {
      LOG(ERROR) << "Error sending data: " << errno;
      break;
    }

    if (HANDLE_EINTR(poll(&fds, 1, kMessageTimeoutSeconds * 1000)) != 1) {
      // Timeout or error.
      LOG(ERROR) << "Timeout or error: " << errno;
      break;
    }
    if (fds.revents & POLLERR) {
      LOG(ERROR) << "Poll error";
      break;
    }
    if (fds.revents & POLLHUP) {
      LOG(ERROR) << "Hang up error";
      break;
    }

    // Since this is over TCP it shouldn't be possible for the 8 byte
    // packet to get segmented, especially that we're disabling Nagle's
    // algorithm.
    const int read_status = HANDLE_EINTR(read(socket, buf_recv, buf_size + 1));
    if (read_status != buf_size) {
      // Timeout or did not receive right# of bytes.
      LOG(ERROR) << "Wrong read. Status: " << read_status
                 << " err: " << errno;
      break;
    }

    if (memcmp(buf, buf_recv, buf_size) != 0) {
      LOG(ERROR) << "Data did not match";
      break;
    }

    succeeded = true;

  } while (false);

  return succeeded;
}

// Every 10 seconds pings the host using the Echo service.
// If a certain number of subsequent attempts fail we will reboot the device.
void* PingThread::WorkerThread(void* arg) {
  PingThread* pingThread = reinterpret_cast<PingThread*>(arg);
  while (!g_killed) {
    if (pingThread->ShouldReboot()) {
      LOG(ERROR) << "PingThread failed.  Rebooting device.";
      DoReboot();
    }

    pingThread->OnRequestIssued();

    int host_socket = tools::ConnectAdbHostSocket("7:127.0.0.1");
    // We need to disable Nagle's algorithm, otherwise ping requests might get
    // queued up and we'll always timeout waiting.
    tools::DisableNagle(host_socket);

    if (host_socket >= 0) {
      fcntl(host_socket, F_SETFL, fcntl(host_socket, F_GETFL) | O_NONBLOCK);
      if (pingThread->DoPing(host_socket)) {
        pingThread->PingSucceeded();
      } else {
        LOG(ERROR) << "PingThread failed (host_socket >=0).";
        pingThread->PingFailed();
      }
      close(host_socket);
    } else {
      LOG(ERROR) << "PingThread failed (host_socket < 0)";
      pingThread->PingFailed();
    }
    sleep(kMessagePeriodSeconds);
  }
  return NULL;
}

void PingThread::DoReboot() {
  LOG(ERROR) << "Too many requests failed.. Rebooting";
  reboot(LINUX_REBOOT_CMD_RESTART);
}

PingThread* g_thread = NULL;

void DumpInformation(int) {
  if (g_thread != NULL) {
    g_thread->DumpInformation();
  }
}

}  // namespace

int main(int argc, char** argv) {
  printf("Keep android alive device-side\n");

  CommandLine command_line(argc, argv);
  if (tools::HasHelpSwitch(command_line) || command_line.GetArgs().size()) {
    tools::ShowHelp(argv[0], "", "");
    return 0;
  }

  if (!tools::HasNoSpawnDaemonSwitch(command_line))
    tools::SpawnDaemon(0);

  signal(SIGUSR2, DumpInformation);

  g_thread = new PingThread();
  g_thread->Start();
  g_thread->Join();
  delete g_thread;

  return 0;
}
