// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_APP_REMOTING_LATENCY_TEST_FIXTURE_H_
#define REMOTING_TEST_APP_REMOTING_LATENCY_TEST_FIXTURE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/test/remote_connection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class RunLoop;
class Timer;
}

namespace webrtc {
class DesktopRect;
}

namespace remoting {
namespace test {

struct RemoteApplicationDetails;
class AppRemotingConnectionHelper;
class TestVideoRenderer;

typedef uint32 RgbaColor;

// Creates a connection to a remote host which is available for tests to use.
// Provides convenient methods to create test cases to measure the input and
// rendering latency between client and the remote host.
// NOTE: This is an abstract class. To use it, please derive from this class
// and initialize the application names in constructor.
class AppRemotingLatencyTestFixture : public testing::Test {
 public:
  AppRemotingLatencyTestFixture();
  ~AppRemotingLatencyTestFixture() override;

 protected:
  // Set expected image pattern for comparison and a matched reply will be
  // called when the pattern is matched.
  void SetExpectedImagePattern(const webrtc::DesktopRect& expected_rect,
                               const RgbaColor& expected_color);

  // Waits for an image pattern matched reply up to |max_wait_time|. Returns
  // true if we received a response within the maximum time limit.
  // NOTE: SetExpectedImagePattern() must be called before calling this method.
  bool WaitForImagePatternMatched(const base::TimeDelta& max_wait_time);

  // Name of the application being tested.
  // NOTE: must be initialized in the constructor of derived class.
  std::string application_name_;

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

  // Creates and manages the connection to the remote host.
  scoped_ptr<AppRemotingConnectionHelper> connection_helper_;

  // Used to ensure RemoteConnectionObserver methods are called on the same
  // thread.
  base::ThreadChecker thread_checker_;

  // Using WeakPtr to keep a reference to TestVideoRenderer while let the
  // TestChromotingClient own its lifetime.
  base::WeakPtr<TestVideoRenderer> test_video_renderer_;

  DISALLOW_COPY_AND_ASSIGN(AppRemotingLatencyTestFixture);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_APP_REMOTING_LATENCY_TEST_FIXTURE_H_
