// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_APP_REMOTING_CONNECTED_CLIENT_FIXTURE_H_
#define REMOTING_TEST_APP_REMOTING_CONNECTED_CLIENT_FIXTURE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/test/remote_connection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class RunLoop;
class Timer;
}

namespace remoting {
namespace test {

struct RemoteApplicationDetails;
class AppRemotingConnectionHelper;

// Creates a connection to a remote host which is available for tests to use.
// All callbacks must occur on the same thread the object was created on.
class AppRemotingConnectedClientFixture
    : public testing::TestWithParam<const char*> {
 public:
  AppRemotingConnectedClientFixture();
  ~AppRemotingConnectedClientFixture() override;

 protected:
  // Sends the request to the host and waits for a reply up to |max_wait_time|.
  // Returns true if we received a response within the maximum time limit.
  bool VerifyResponseForSimpleHostMessage(
      const std::string& message_request_title,
      const std::string& message_response_title,
      const std::string& message_payload,
      const base::TimeDelta& max_wait_time);

 private:
  // testing::Test interface.
  void SetUp() override;
  void TearDown() override;

  // Contains the details for the application being tested.
  const RemoteApplicationDetails& application_details_;

  // Used to run the thread's message loop.
  scoped_ptr<base::RunLoop> run_loop_;

  // Used for setting timeouts and delays.
  scoped_ptr<base::Timer> timer_;

  // Used to ensure RemoteConnectionObserver methods are called on the same
  // thread.
  base::ThreadChecker thread_checker_;

  // Creates and manages the connection to the remote host.
  scoped_ptr<AppRemotingConnectionHelper> connection_helper_;

  DISALLOW_COPY_AND_ASSIGN(AppRemotingConnectedClientFixture);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_APP_REMOTING_CONNECTED_CLIENT_FIXTURE_H_
