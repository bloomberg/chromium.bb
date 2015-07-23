// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_APP_REMOTING_LATENCY_TEST_FIXTURE_H_
#define REMOTING_TEST_APP_REMOTING_LATENCY_TEST_FIXTURE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/test/remote_connection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class RunLoop;
class Timer;
class TimeDelta;
}

namespace webrtc {
class DesktopRect;
}

namespace remoting {
namespace test {

struct RemoteApplicationDetails;
class AppRemotingConnectionHelper;
class TestVideoRenderer;

// Called to wait for expected image pattern to be matched within up to a max
// wait time.
typedef base::Callback<bool(const base::TimeDelta& max_wait_time)>
    WaitForImagePatternMatchCallback;

// Creates a connection to a remote host which is available for tests to use.
// Provides convenient methods to create test cases to measure the input and
// rendering latency between client and the remote host.
// NOTE: This is an abstract class. To use it, please derive from this class
// and implement GetApplicationDetails to specify the application details.
class AppRemotingLatencyTestFixture : public testing::Test {
 public:
  AppRemotingLatencyTestFixture();
  ~AppRemotingLatencyTestFixture() override;

 protected:
  // Set expected image pattern for comparison.
  // A WaitForImagePatternMatchCallback is returned to allow waiting for the
  // expected image pattern to be matched.
  WaitForImagePatternMatchCallback SetExpectedImagePattern(
      const webrtc::DesktopRect& expected_rect,
      uint32_t expected_avg_color);

  // Inject press & release key event.
  void PressAndReleaseKey(uint32_t usb_keycode);

  // Inject press & release a combination of key events.
  void PressAndReleaseKeyCombination(const std::vector<uint32_t>& usb_keycodes);

  // Clean up the running application to initial state.
  virtual void ResetApplicationState();

  // Get the details of the application to be run.
  virtual const RemoteApplicationDetails& GetApplicationDetails() = 0;

  // Creates and manages the connection to the remote host.
  scoped_ptr<AppRemotingConnectionHelper> connection_helper_;

 private:
  // testing::Test interface.
  void SetUp() override;
  void TearDown() override;

  // Inject press key event.
  void PressKey(uint32_t usb_keycode, bool pressed);

  // Waits for an image pattern matched reply up to |max_wait_time|. Returns
  // true if we received a response within the maximum time limit.
  // NOTE: This method should only be run when as a returned callback by
  // SetExpectedImagePattern.
  bool WaitForImagePatternMatch(scoped_ptr<base::RunLoop> run_loop,
                                const base::TimeDelta& max_wait_time);

  // Used to run the thread's message loop.
  scoped_ptr<base::RunLoop> run_loop_;

  // Used for setting timeouts and delays.
  scoped_ptr<base::Timer> timer_;

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
