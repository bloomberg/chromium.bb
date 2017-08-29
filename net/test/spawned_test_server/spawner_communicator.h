// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SPAWNED_TEST_SERVER_SPAWNER_COMMUNICATOR_H_
#define NET_TEST_SPAWNED_TEST_SERVER_SPAWNER_COMMUNICATOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "net/test/spawned_test_server/remote_test_server_config.h"
#include "net/url_request/url_request.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace net {

class ScopedPortException;

// SpawnerCommunicator communicates with a spawner server that runs on a
// remote system.
//
// The test server used by unit tests is written in Python. However, Android
// does not support running Python code, so the test server cannot run on the
// same device running unit tests.
//
// The actual test server is executed on the host machine, while the unit tests
// themselves continue running on the device. To control the test server on the
// host machine, a second HTTP server is started, the spawner server, which
// controls the life cycle of remote test servers. Calls to start/kill the
// SpawnedTestServer are then redirected to the spawner server via
// this spawner communicator.
//
// Currently only three commands are supported by spawner.
//
// (1) Start Python test server, format is:
// Path: "/start".
// Method: "POST".
// Data to server: all arguments needed to launch the Python test server, in
//   JSON format.
// Data from server: a JSON dict includes the following two field if success,
//   "port": the port the Python test server actually listen on that.
//   "message": must be "started".
//
// (2) Kill Python test server, format is:
// Path: "/kill".
// Method: "GET".
// Data to server: port=<server_port>.
// Data from server: String "killed" returned if success.
//
// (3) Ping Python test server to see whether it is alive, format is:
// Path: "/ping".
// Method: "GET".
// Data to server: None.
// Data from server: String "ready" returned if success.
//
// The internal I/O thread is required by net stack to perform net I/O.
// The Start/StopServer methods block the caller thread until result is
// fetched from spawner server or timed-out.
class SpawnerCommunicator : public URLRequest::Delegate {
 public:
  explicit SpawnerCommunicator(const RemoteTestServerConfig& config);
  ~SpawnerCommunicator() override;

  // Starts an instance of the Python test server on the host/ machine.If
  // successfully started, returns true, setting |*server_data| to the server
  // data returned by the spawner.
  bool StartServer(const std::string& arguments,
                   std::string* server_data) WARN_UNUSED_RESULT;
  bool StopServer(uint16_t port) WARN_UNUSED_RESULT;

 private:
  // Starts the IO thread. Called on the user thread.
  void StartIOThread();

  // Shuts down the remote test server spawner. Called on the user thread.
  void Shutdown();

  // Waits for the server response on IO thread. Called on the user thread.
  void WaitForResponse();

  // Sends a command to the test server over HTTP, returning response data in
  // |*data_received|, which must not be nullptr. If |post_data| is empty, HTTP
  // GET will be used to send |command|. If |post_data| is non-empty, performs
  // an HTTP POST. This method is called on the user thread.
  int SendCommandAndWaitForResult(const std::string& command,
                                  const std::string& post_data,
                                  std::string* data_received);

  // Performs the command sending on the IO thread. Called on the IO thread.
  void SendCommandAndWaitForResultOnIOThread(const std::string& command,
                                             const std::string& post_data,
                                             int* result_code,
                                             std::string* data_received);

  // URLRequest::Delegate methods. Called on the IO thread.
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int num_bytes) override;

  // Reads Result from the response. Called on the IO thread.
  void ReadResult(URLRequest* request);

  // Called on the IO thread upon completion of the spawner command.
  void OnSpawnerCommandCompleted(URLRequest* request, int net_error);

  // Timeout timer task. Runs on IO thread.
  void OnTimeout();

  const RemoteTestServerConfig config_;

  // A thread to communicate with test_spawner server.
  base::Thread io_thread_;

  // WaitableEvent to notify whether the communication is done.
  base::WaitableEvent event_;

  // Helper to add spawner port to the list of the globally explicitly allowed
  // ports.
  std::unique_ptr<ScopedPortException> allowed_port_;

  // Request context used by |cur_request_|.
  std::unique_ptr<URLRequestContext> context_;

  // The current (in progress) request, or NULL.
  std::unique_ptr<URLRequest> cur_request_;

  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SpawnerCommunicator);
};

}  // namespace net

#endif  // NET_TEST_SPAWNED_TEST_SERVER_SPAWNER_COMMUNICATOR_H_
